#include <Arduino.h>
#include "settings.h"

#include "Message.h"

#include "AudioPlayer.h"
#include "Log.h"

void Message_Send(Msg &&msg) {
	// message distribution hub
	switch (msg.type()) {
		case Msg::AudioPlayerMsg:
            AudioPlayer_SignalMessage(std::move(msg));
			break;

		default:
			Log_Printf(LOGLEVEL_ERROR, "Message with the type %d not understood", std::to_underlying(msg.type()));
			break;
	}
}
