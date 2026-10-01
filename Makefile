EE_BIN = 500_Card_Game_PS2_54CARD_960x540_X2.ELF
EE_OBJS = main.o assets.o audsrv_irx.o sio2man_irx.o padman_irx.o
EE_INCS += -I$(GSKIT)/include -I$(PS2SDK)/ports/include
EE_LDFLAGS += -L$(GSKIT)/lib -L$(PS2SDK)/ports/lib -Wl,--gc-sections
EE_LIBS = -lgskit -ldmakit -laudsrv -lpadx -lpatches -lz -lm -lc
EE_OPTFLAGS = -Os -ffunction-sections -fdata-sections
EE_DBGINFOFLAGS =

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal