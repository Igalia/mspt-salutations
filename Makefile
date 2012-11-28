# MSPT Salutations makefile

BIN=mspt-salutations

all: mspt-salutations

mspt-salutations: Makefile main.c \
	video-player.c video-player.h \
	transition.c transition.h \
	storyboard.c storyboard.h \
	salut.c salut.h \
	salut-stream.c salut-stream.h
	@cc -O2 -ggdb -Wall \
		`pkg-config --libs --cflags glib-2.0 gstreamer-0.10 clutter-1.0 clutter-gst-1.0 gfreenect-0.1 skeltrack-0.1 opencv` \
		-o ${BIN} \
		main.c \
		video-player.c \
		transition.c \
		storyboard.c \
		salut.c \
		salut-stream.c

clean:
	@rm ${BIN}

run:
	./${BIN}
