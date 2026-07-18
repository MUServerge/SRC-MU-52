# Verified protocol topology

## Client entry points

- `source/WSclient.cpp` and `source/WSclient.h`: legacy receive compiler, the large `TranslateProtocol` dispatcher, and most packet types/handlers.
- `source/wsclientinline.h`: legacy send construction with `CStreamPacketEngine`.
- `source/StreamPacketEngine.h`: 2048-byte builder, C1/C2 headers, XOR, and send handoff.
- `source/Protocol.cpp` and `source/Protocol.h`: `ProtocolCoreEx` custom dispatch.
- `source/NewUIReconnect.cpp` and `.h`: reconnect state and restoration flow.
- `source/CSMapServer.cpp` and `.h`: map-server movement integration.

Legacy receive accepts C1/C2 plaintext and C3/C4 encrypted frames, decrypts where required, and sends the decoded packet to `TranslateProtocol`. Many handlers cast the raw buffer directly to packet structs; audit declared and available size before every cast, especially count-based arrays.

## Experimental parallel client path

- `source/Defined_Global.h` contains a commented `NEW_PROTOCOL_SYSTEM` define.
- `source/ProtocolSend.cpp/.h` and `source/ProtocolAsio.h` implement an ASIO message wrapper.
- `BOTH_MESSAGE` copies a wrapped legacy packet into a fixed 8024-byte stack buffer and calls `TranslateProtocol` again.

This is not a complete replacement protocol. Before enabling it, bound `header.size` against the actual body and destination, require a valid legacy header and exact declared size, guard vector pop underflow, prove server compatibility, and preserve legacy encryption/serial semantics.

## GameServer boundary

- `Source/GameServer/GameServer/SocketManager.cpp`: IOCP receive framing, decrypt, queue, and send.
- `PacketManager.cpp/.h`: 2048-byte framing buffer, decrypt/encrypt, XOR, and extraction.
- `Protocol.cpp/.h`: `ProtocolCore` head/subhead dispatch.
- `HackPacketCheck.cpp/.h`: configured head/subhead, encryption, serial, timing, and rate checks.
- `Reconnect.cpp/.h`: in-memory reconnect records and state restoration.

Structural packet validation must remain independent from `HackPacketCheck`; encryption, serial, and rate checks do not prove the concrete struct is present. Recheck minimum remaining header bytes on every framing-loop iteration before reading size/head.

## Internal service boundaries

- GameServer `DSProtocol.*` ↔ DataServer `DataServerProtocol.*`.
- GameServer `JSProtocol.*` ↔ JoinServer `JoinServerProtocol.*`.
- GameServer `CSProtocol.*` ↔ ConnectServer protocol handlers.

The current client project defines `MAIN_UPDATE=603` and `PROTO_EXTRA`. Service `stdafx.h` files contain 803 fallbacks, but the Visual Studio projects override them per configuration: GameServer includes EX603, EX401, and EX803 variants, and DataServer likewise has versioned variants. The active `.vcxproj` configuration—not the header fallback—determines the wire layout. Pair the client with the matching GameServer/DataServer configuration and record that selection in a build compatibility manifest before changing shared data.

## Packet parity template

| Field | Evidence |
|---|---|
| Operation and direction | |
| Lifecycle state | |
| C1/C2/C3/C4 and encryption | |
| Head/subhead | |
| Declared/min/exact size | |
| Count, entry size, max | |
| Client sender/receiver | |
| GameServer handler/authority | |
| DS/JS/CS request/response | |
| Database read/write | |
| Packing/version guards | |
| Retry/duplicate/rollback | |
| Tests performed | |
