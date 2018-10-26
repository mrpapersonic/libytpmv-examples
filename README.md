# YTPMV examples
Sample ytpmvs using libytpmv.

To play an example, run:

```bash
export LIBYTPMV_DIR=/path/to/libytpmv
make example1 -j$(nproc)
./example1
```
Make sure all OpenGL and GLFW development files are installed. On debian based systems these are:
```
libglew-dev libglfw3-dev libglm-dev libgles2-mesa-dev
```

If your system can not support OpenGL you can play the YTPMVs in audio only mode:
```bash
./example1 playaudio
```

**example1.C**
- Song: hehj (https://modarchive.org/index.php?request=view_by_moduleid&query=106629)
- Video: https://www.youtube.com/watch?v=lsV5rRjUIqg

**example2.C**
- Song: micronat - flying (https://modarchive.org/index.php?request=view_by_moduleid&query=181002)
- Video: https://www.youtube.com/watch?v=_ZYiu_H_uBQ

**example3.C**
- Song: micronat & ewk - castle (https://modarchive.org/index.php?request=view_by_moduleid&query=180205)
- Video: https://www.youtube.com/watch?v=6CgBfAdhhNU

**example4.C**
- Song: madotsuki - This Song Is Sad, I Swear (https://modarchive.org/index.php?request=view_by_moduleid&query=182096)
- Video: https://www.youtube.com/watch?v=r6-__GHV6Mw

**r3c.C**
- collab entry for robocop3 (https://modarchive.org/index.php?request=view_by_moduleid&query=136815)
- Video: https://www.youtube.com/watch?v=sEJqZmx8UnY
