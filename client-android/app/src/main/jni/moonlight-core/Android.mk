# Android.mk for moonlight-core and binding
MY_LOCAL_PATH := $(call my-dir)

include $(call all-subdir-makefiles)

LOCAL_PATH := $(MY_LOCAL_PATH)

include $(CLEAR_VARS)
LOCAL_MODULE    := moonlight-core

# CloudDeck-Client: moonlight-common-c lives once at the repo root (see docs/adr/0001)
MLC := ../../../../../../common

LOCAL_SRC_FILES := $(MLC)/src/AudioStream.c \
                   $(MLC)/src/MicrophoneStream.c \
                   $(MLC)/src/ByteBuffer.c \
                   $(MLC)/src/Connection.c \
                   $(MLC)/src/ConnectionTester.c \
                   $(MLC)/src/ControlStream.c \
                   $(MLC)/src/FakeCallbacks.c \
                   $(MLC)/src/InputStream.c \
                   $(MLC)/src/LinkedBlockingQueue.c \
                   $(MLC)/src/Misc.c \
                   $(MLC)/src/Platform.c \
                   $(MLC)/src/PlatformCrypto.c \
                   $(MLC)/src/PlatformSockets.c \
                   $(MLC)/src/RtpAudioQueue.c \
                   $(MLC)/src/RtpVideoQueue.c \
                   $(MLC)/src/RtspConnection.c \
                   $(MLC)/src/RtspParser.c \
                   $(MLC)/src/SdpGenerator.c \
                   $(MLC)/src/SimpleStun.c \
                   $(MLC)/src/VideoDepacketizer.c \
                   $(MLC)/src/VideoStream.c \
                   $(MLC)/nanors/deps/obl/oblas_common.c \
                   $(MLC)/nanors/deps/obl/oblas_lite.c \
                   $(MLC)/nanors/rs.c \
                   $(MLC)/enet/callbacks.c \
                   $(MLC)/enet/compress.c \
                   $(MLC)/enet/host.c \
                   $(MLC)/enet/list.c \
                   $(MLC)/enet/packet.c \
                   $(MLC)/enet/peer.c \
                   $(MLC)/enet/protocol.c \
                   $(MLC)/enet/unix.c \
                   $(MLC)/enet/win32.c \
                   simplejni.c \
                   callbacks.c \
                   minisdl.c \


LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(MLC)/enet/include \
                    $(LOCAL_PATH)/$(MLC)/nanors/deps/obl \
                    $(LOCAL_PATH)/$(MLC)/nanors \
                    $(LOCAL_PATH)/$(MLC)/src \

LOCAL_CFLAGS := -DHAS_SOCKLEN_T=1 -DLC_ANDROID -DHAVE_CLOCK_GETTIME=1

ifeq ($(NDK_DEBUG),1)
LOCAL_CFLAGS += -DLC_DEBUG
endif

LOCAL_LDLIBS := -llog

LOCAL_STATIC_LIBRARIES := libopus libcrypto cpufeatures
LOCAL_LDFLAGS += -Wl,--exclude-libs,ALL

LOCAL_BRANCH_PROTECTION := standard

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/cpufeatures)