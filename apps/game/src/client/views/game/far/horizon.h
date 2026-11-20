// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/terrain.h"

class NetworkPacket;

// TOCLIENT_HORIZON's body: a far::Horizon, field for field. Numbers the
// species' choice compares are sent as their exact bits.
namespace far {

void write(NetworkPacket &pkt, const Horizon &h);
// Throws SerializationError if the server's terragen doesn't match ours.
Horizon read(NetworkPacket &pkt);

} // namespace far
