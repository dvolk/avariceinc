SANITIZE=-fsanitize=address -fsanitize=leak -fsanitize=undefined
override CXXFLAGS+=-g -Wall -Wextra -pedantic --std=c++11 $(SANITIZE) -funsigned-char -Wno-unused-parameter
#override CXX=clang++-3.5
INCLUDES=

LIBS=-lstdc++ `pkg-config --libs allegro-5.0 allegro_primitives-5.0 allegro_color-5.0 allegro_image-5.0 allegro_font-5.0 allegro_ttf-5.0 allegro_dialog-5.0 allegro_audio-5.0 allegro_acodec-5.0 gl`

OBJS= \
	src/util.o src/colors.o src/config.o src/widget.o src/ui.o src/button.o src/sidebutton.o src/main.o

default: all

version:
	@( VERSION_STRING=$(VERSION) ; \
            [ -e "./.git" ] && GITVERSION=$$( git describe --tags --always --dirty --match "[0-9A-Z]*.[0-9A-Z]*" ) && VERSION_STRING=$$GITVERSION ; \
            [ -e "src/version.h" ] && OLDVERSION=$$(grep VERSION src/version.h|cut -d '"' -f2) ; \
            if [ "x$$VERSION_STRING" != "x$$OLDVERSION" ]; then echo "#define VERSION \"$$VERSION_STRING\"" | tee src/version.h ; fi \
         )

all: version $(OBJS)
	$(CXX) $(SANITIZE) -o ./avariceinc $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	-$(RM) $(OBJS) src/version.h ./avariceinc
