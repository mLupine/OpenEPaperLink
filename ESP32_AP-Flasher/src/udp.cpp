#include "udp.h"

#include <Arduino.h>
#include <WiFi.h>

#include "AsyncUDP.h"
#include "commstructs.h"
#include "newproto.h"
#include "serialap.h"
#include "tag_db.h"
#include "web.h"
#include "wifimanager.h"

#define UDPIP IPAddress(239, 10, 0, 1)
#define UDPPORT 16033

UDPcomm udpsync;

extern uint8_t channelList[6];
extern espSetChannelPower curChannel;

void init_udp() {
    Serial.println("[UDP] init_udp() called from WiFi event");
    udpsync.init();
    Serial.println("[UDP] init_udp() completed");
}

UDPcomm::UDPcomm() {
    // Constructor
}

UDPcomm::~UDPcomm() {
    // Destructor
}

void UDPcomm::init() {
    const char* interface = (wm.wifiStatus == ETHERNET) ? "ETHERNET" : "WIFI";
    Serial.printf("[UDP] Initializing UDP discovery on %s. Mode: %s (config.discovery=%d)\n",
                  interface, config.discovery == 0 ? "MULTICAST" : "BROADCAST", config.discovery);
    Serial.printf("[UDP] Local IP: %s\n", wm.localIP().toString().c_str());

    if (config.discovery == 0) {
        Serial.printf("[UDP] Attempting to listen on multicast %s:%d\n", UDPIP.toString().c_str(), UDPPORT);
        if (udp.listenMulticast(UDPIP, UDPPORT)) {
            Serial.println("[UDP] ✓ Multicast listen succeeded");
            udp.onPacket([this](AsyncUDPPacket packet) {
                if (packet.remoteIP() != wm.localIP()) {
                    this->processPacket(packet);
                }
            });
        } else {
            Serial.println("[UDP] ✗ Multicast listen FAILED");
        }
    } else {
        Serial.printf("[UDP] Attempting to listen on broadcast port %d\n", UDPPORT);
        if (udp.listen(UDPPORT)) {
            Serial.println("[UDP] ✓ Broadcast listen succeeded");
            udp.onPacket([this](AsyncUDPPacket packet) {
                if (packet.isBroadcast() && packet.remoteIP() != wm.localIP()) {
                    this->processPacket(packet);
                }
            });
        } else {
            Serial.println("[UDP] ✗ Broadcast listen FAILED");
        }
    }
    setAPchannel();
}

void UDPcomm::processPacket(AsyncUDPPacket packet) {
    if (config.runStatus == RUNSTATUS_STOP) {
        return;
    }
    IPAddress senderIP = packet.remoteIP();

    switch (packet.data()[0]) {
        case PKT_AVAIL_DATA_INFO: {
            espAvailDataReq adr;
            memset(&adr, 0, sizeof(espAvailDataReq));
            memcpy(&adr, &packet.data()[1], std::min(packet.length() - 1, sizeof(espAvailDataReq)));
            processDataReq(&adr, false, senderIP);
            break;
        }
        case PKT_XFER_COMPLETE: {
            espXferComplete xfc;
            memset(&xfc, 0, sizeof(espXferComplete));
            memcpy(&xfc, &packet.data()[1], std::min(packet.length() - 1, sizeof(espXferComplete)));
            processXferComplete(&xfc, false);
            break;
        }
        case PKT_XFER_TIMEOUT: {
            espXferComplete xfc;
            memset(&xfc, 0, sizeof(espXferComplete));
            memcpy(&xfc, &packet.data()[1], std::min(packet.length() - 1, sizeof(espXferComplete)));
            processXferTimeout(&xfc, false);
            break;
        }
        case PKT_AVAIL_DATA_REQ: {
            pendingData pending;
            memset(&pending, 0, sizeof(pendingData));
            memcpy(&pending, &packet.data()[1], std::min(packet.length() - 1, sizeof(pendingData)));
            prepareExternalDataAvail(&pending, senderIP);
            break;
        }
        case PKT_APLIST_REQ: {
            APlist APitem;
            APitem.src = wm.localIP();
            strcpy(APitem.alias, config.alias);
            APitem.channelId = curChannel.channel;
            APitem.tagCount = getTagCount();
            APitem.version = apInfo.version;

            uint8_t buffer[sizeof(struct APlist) + 1];
            buffer[0] = PKT_APLIST_REPLY;
            memcpy(buffer + 1, &APitem, sizeof(struct APlist));
            writeUdpPacket(buffer, sizeof(buffer), senderIP);
            break;
        }
        case PKT_APLIST_REPLY: {
            APlist APreply;
            memset(&APreply, 0, sizeof(APlist));
            memcpy(&APreply, &packet.data()[1], std::min(packet.length() - 1, sizeof(APlist)));
            Serial.printf("[UDP] AP discovered: %s (%s) ch:%d tags:%d\n",
                          ((IPAddress)APreply.src).toString().c_str(), APreply.alias,
                          APreply.channelId, APreply.tagCount);
            for (int i = 0; i < 6; i++) {
                if (channelList[i] == APreply.channelId) channelList[i] = 0;
            }
            wsSendAPitem(&APreply);
            break;
        }
        case PKT_TAGINFO: {
            uint16_t syncversion = (packet.data()[2] << 8) | packet.data()[1];
            if (syncversion != SYNC_VERSION) {
                wsErr("Got a packet from " + senderIP.toString() + " with mismatched udp sync version. Update firmware!");
            } else {
                TagInfo* taginfoitem = (TagInfo*)&packet.data()[1];
                updateTaginfoitem(taginfoitem, senderIP);
            }
        }
    }
}

void autoselect(void* pvParameters) {
    // reset channel list
    uint8_t values[] = {11, 15, 20, 25, 26, 27};
    memcpy(channelList, values, sizeof(values));
    // wait 5s for channelList to collect all AP's
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    curChannel.channel = 0;
    for (int i = 0; i < 6; i++) {
        if (channelList[i] > 0) {
            curChannel.channel = channelList[i];
            break;
        }
    }
    if (curChannel.channel == 0) {
        curChannel.channel = 11;
    }
    config.channel = curChannel.channel;
    do {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } while (!apInfo.isOnline);

    sendChannelPower(&curChannel);
    saveAPconfig();

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelete(NULL);
}

void UDPcomm::getAPList() {
    APlist APitem;
    APitem.src = wm.localIP();
    strcpy(APitem.alias, config.alias);
    APitem.channelId = curChannel.channel;
    APitem.tagCount = getTagCount();
    APitem.version = apInfo.version;
    wsSendAPitem(&APitem);

    if (config.alias == 0) {
        xTaskCreate(autoselect, "autoselect", 5000, NULL, 2, NULL);
    }

    uint8_t buffer[sizeof(struct APlist) + 1];
    buffer[0] = PKT_APLIST_REQ;
    memcpy(buffer + 1, &APitem, sizeof(struct APlist));
    writeUdpPacket(buffer, sizeof(buffer), UDPIP);
}

void UDPcomm::netProcessDataReq(struct espAvailDataReq* eadr) {
    uint8_t buffer[sizeof(struct espAvailDataReq) + 1];
    buffer[0] = PKT_AVAIL_DATA_INFO;
    memcpy(buffer + 1, eadr, sizeof(struct espAvailDataReq));
    writeUdpPacket(buffer, sizeof(buffer), UDPIP);
}

void UDPcomm::netProcessXferComplete(struct espXferComplete* xfc) {
    uint8_t buffer[sizeof(struct espXferComplete) + 1];
    buffer[0] = PKT_XFER_COMPLETE;
    memcpy(buffer + 1, xfc, sizeof(struct espXferComplete));
    writeUdpPacket(buffer, sizeof(buffer), UDPIP);
}

void UDPcomm::netProcessXferTimeout(struct espXferComplete* xfc) {
    uint8_t buffer[sizeof(struct espXferComplete) + 1];
    buffer[0] = PKT_XFER_TIMEOUT;
    memcpy(buffer + 1, xfc, sizeof(struct espXferComplete));
    writeUdpPacket(buffer, sizeof(buffer), UDPIP);
}

void UDPcomm::netSendDataAvail(struct pendingData* pending) {
    uint8_t buffer[sizeof(struct pendingData) + 1];
    buffer[0] = PKT_AVAIL_DATA_REQ;
    memcpy(buffer + 1, pending, sizeof(struct pendingData));
    writeUdpPacket(buffer, sizeof(buffer), UDPIP);
}

void UDPcomm::netTaginfo(struct TagInfo* taginfoitem) {
    uint8_t buffer[sizeof(struct TagInfo) + 1];
    buffer[0] = PKT_TAGINFO;
    memcpy(buffer + 1, taginfoitem, sizeof(struct TagInfo));
    writeUdpPacket(buffer, sizeof(buffer), UDPIP);
}

void UDPcomm::writeUdpPacket(uint8_t *buffer, uint16_t len, IPAddress senderIP) {
    if (config.discovery == 0) {
        udp.writeTo(buffer, len, senderIP, UDPPORT);
    } else {
        udp.broadcastTo(buffer, len, UDPPORT);
    }
}
