set OUT=../bin
glslangValidator.exe -V spv.test.vert -o %OUT%/vert.spv
glslangValidator.exe -V spv.test.frag -o %OUT%/frag.spv
glslangValidator.exe -V spv.rtt.vert -o %OUT%/rtt.vert
glslangValidator.exe -V spv.rtt.frag -o %OUT%/rtt.frag
glslangValidator.exe -V spv.light.vert -o %OUT%/light.vert
glslangValidator.exe -V spv.light.frag -o %OUT%/light.frag
glslangValidator.exe -V spv.shadow.vert -o %OUT%/shadow.vert
glslangValidator.exe -V spv.screenquad.vert -o %OUT%/screenquad.vert
glslangValidator.exe -V spv.fxaa.frag -o %OUT%/fxaa.frag
glslangValidator.exe -V spv.ui2d.vert -o %OUT%/ui2d.vert
glslangValidator.exe -V spv.ui2d.frag -o %OUT%/ui2d.frag
glslangValidator.exe -V spv.normal.vert -o %OUT%/normal.vert
glslangValidator.exe -V spv.normal.frag -o %OUT%/normal.frag
glslangValidator.exe -V spv.pointlight.vert -o %OUT%/pointlight.vert
glslangValidator.exe -V spv.pointlight.frag -o %OUT%/pointlight.frag
glslangValidator.exe -V spv.skybox.vert -o %OUT%/skybox.vert
glslangValidator.exe -V spv.skybox.frag -o %OUT%/skybox.frag
glslangValidator.exe -V spv.smokeupdate.comp -o %OUT%/smokeupdate.comp
glslangValidator.exe -V spv.bilboard.vert -o %OUT%/bilboard.vert
glslangValidator.exe -V spv.bilboard.frag -o %OUT%/bilboard.frag
glslangValidator.exe -V spv.uivector3d.vert -o %OUT%/uivector3d.vert
glslangValidator.exe -V spv.uivector3d.frag -o %OUT%/uivector3d.frag
glslangValidator.exe -V spv.hdrgamma.frag -o %OUT%/hdrgamma.frag
glslangValidator.exe -V spv.pickid.vert -o %OUT%/pickid.vert
glslangValidator.exe -V spv.pickid.frag -o %OUT%/pickid.frag
glslangValidator.exe -V spv.pickbb.vert -o %OUT%/pickbb.vert
glslangValidator.exe -V spv.pickbb.frag -o %OUT%/pickbb.frag
glslangValidator.exe -V spv.ssao.frag -o %OUT%/ssao.frag
glslangValidator.exe -V spv.hblur.frag -o %OUT%/hblur.frag
glslangValidator.exe -V spv.vblur.frag -o %OUT%/vblur.frag
glslangValidator.exe -V spv.lineardepth.frag -o %OUT%/lineardepth.frag
glslangValidator.exe -V spv.fog.frag -o %OUT%/fog.frag
glslangValidator.exe -V spv.sun.vert -o %OUT%/sun.vert
glslangValidator.exe -V spv.sun.frag -o %OUT%/sun.frag
glslangValidator.exe -V spv.extractlight.frag -o %OUT%/extractlight.frag
glslangValidator.exe -V spv.radialblur.frag -o %OUT%/radialblur.frag
glslangValidator.exe -V spv.passtrough.frag -o %OUT%/passtrough.frag
glslangValidator.exe -V spv.render3Dtexture.frag -o %OUT%/render3Dtexture.frag
glslangValidator.exe -V spv.volume.geom -o %OUT%/volume.geom
glslangValidator.exe -V spv.sample3Dtexture.frag -o %OUT%/sample3Dtexture.frag

pause