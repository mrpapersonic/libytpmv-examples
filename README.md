# YTPMV examples
Sample ytpmvs using libytpmv.

**example9.C**
- Song: ohayou sayori (ddlc)
- Video: https://www.youtube.com/watch?v=6WBrY08Mt-Q

**example8.C**
- Song: home alone (https://modarchive.org/index.php?request=view_by_moduleid&query=122611)
- Video: https://www.youtube.com/watch?v=oZs7pZt1Dlk

**example7.C**
- Song: kitsune^2 - skuggin' around
- Video: https://www.youtube.com/watch?v=aGYoEKGbE-o

**example6.C**
- Song: metal beat (https://modarchive.org/index.php?request=view_by_moduleid&query=179905)
- Video: https://www.youtube.com/watch?v=ZJYdS1p4xWc

**example5.C**
- Song: xaxaxa - untitled (https://modarchive.org/index.php?request=view_by_moduleid&query=181966)
- Video: https://www.youtube.com/watch?v=mYS2WRwnBdc

**example4.C**
- Song: madotsuki - This Song Is Sad, I Swear (https://modarchive.org/index.php?request=view_by_moduleid&query=182096)
- Video: https://www.youtube.com/watch?v=r6-__GHV6Mw

**example3.C**
- Song: micronat & ewk - castle (https://modarchive.org/index.php?request=view_by_moduleid&query=180205)
- Video: https://www.youtube.com/watch?v=6CgBfAdhhNU

**example2.C**
- Song: micronat - flying (https://modarchive.org/index.php?request=view_by_moduleid&query=181002)
- Video: https://www.youtube.com/watch?v=_ZYiu_H_uBQ

**example1.C**
- Song: hehj (https://modarchive.org/index.php?request=view_by_moduleid&query=106629)
- Video: https://www.youtube.com/watch?v=lsV5rRjUIqg

**example0.C**
- Song: edzes - inside beek's mind (https://modarchive.org/index.php?request=view_by_moduleid&query=41017)
- Video: https://www.youtube.com/watch?v=QeSnbP2Y3ac

**r3c.C**
- collab entry for robocop3 (https://modarchive.org/index.php?request=view_by_moduleid&query=136815)
- Video: https://www.youtube.com/watch?v=sEJqZmx8UnY


**To play an example, run:**

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
