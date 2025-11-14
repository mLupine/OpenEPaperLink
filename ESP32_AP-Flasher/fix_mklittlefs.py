Import("env")
import os

home_dir = os.path.expanduser("~")
mklittlefs_path = os.path.join(
    home_dir,
    ".platformio",
    "packages",
    "tool-mklittlefs@src-e7535062600f3b3519eeaed550c42e66",
    "mklittlefs"
)

if os.path.exists(mklittlefs_path):
    env.Replace(MKFSTOOL=mklittlefs_path)
    env.PrependENVPath("PATH", os.path.dirname(mklittlefs_path))
    print(f"Using mklittlefs from: {mklittlefs_path}")
else:
    print(f"WARNING: mklittlefs not found at {mklittlefs_path}")
