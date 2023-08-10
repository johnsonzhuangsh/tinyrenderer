#!/bin/bash

cd build
rm render_target.tga 2>&1 /dev/null
./tinyrenderer ../obj/diablo3_pose/diablo3_pose.obj ../obj/floor.obj
cd ..