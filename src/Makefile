OPENCV_LINK = -isystem ${SDKTARGETSYSROOT}/usr/include/opencv4 \
			  -lopencv_imgcodecs -lopencv_imgproc -lopencv_core -lopencv_highgui

BSP_080_SDK_FLAG = \
			  -ljpeg -lwebp -ltiff -lz -ltbb -lgtk-3 -lpng16 -lgdk-3 -lcairo  \
			  -llzma -lrt -lcairo-gobject \
			  -lxkbcommon -lwayland-cursor -lwayland-egl -lwayland-client -lepoxy \
			  -lfribidi -lharfbuzz -lfontconfig \
			  -lglib-2.0 -lgobject-2.0 -lgdk_pixbuf-2.0 -lgmodule-2.0 -lpangocairo-1.0 \
			  -latk-1.0 -lgio-2.0 -lpango-1.0 -lfreetype -lpixman-1 -luuid -lpcre \
			  -lmount -lresolv -lexpat -lpangoft2-1.0 -lblkid \

all: sample_app_cam_mipi_vga \
      sample_app_cam_mipi_720p \
      sample_app_cam_mipi_1080p 
      
sample_app_cam_mipi_vga: sample_app_cam.cpp
	${CXX} -std=c++14 sample_app_cam.cpp camera.cpp image.cpp wayland.cpp \
	-DCORAL_CAM \
	-lwayland-client \
	${OPENCV_LINK} ${BSP_080_SDK_FLAG} \
	-lpthread -O2 -ldl ${LDFLAGS} -o bin/sample_app_cam_mipi_vga
	
sample_app_cam_mipi_720p: sample_app_cam.cpp
	${CXX} -std=c++14 sample_app_cam.cpp camera.cpp image.cpp wayland.cpp \
	-DCORAL_CAM -DCAM_RES_720P \
	-lwayland-client \
	${OPENCV_LINK} ${BSP_080_SDK_FLAG} \
	-lpthread -O2 -ldl ${LDFLAGS} -o bin/sample_app_cam_mipi_720p
	
sample_app_cam_mipi_1080p: sample_app_cam.cpp
	${CXX} -std=c++14 sample_app_cam.cpp camera.cpp image.cpp wayland.cpp \
	-DCORAL_CAM -DCAM_RES_1080P \
	-lwayland-client \
	${OPENCV_LINK} ${BSP_080_SDK_FLAG} \
	-lpthread -O2 -ldl ${LDFLAGS} -o bin/sample_app_cam_mipi_1080p

clean:
	rm -rf *.o bin/*
