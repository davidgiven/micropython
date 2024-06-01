import vfs, tc32

# Try to mount the filesystem, and format the flash if it doesn't exist.
# Note: the flash requires the programming size to be aligned to 256 bytes.
bdev = tc32.Flash()
try:
    fs = vfs.VfsLfs1(bdev, progsize=256)
except:
    print("vfs: formatting LFS filesystem")
    vfs.VfsLfs1.mkfs(bdev, progsize=256)
    fs = vfs.VfsLfs1(bdev, progsize=256)
vfs.mount(fs, "/")

del vfs, bdev, fs, tc32
