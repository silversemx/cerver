#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef PACKETS_DEBUG
#include <errno.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>

#include "cerver/types/types.h"
#include "cerver/types/estring.h"

#include "cerver/network.h"
#include "cerver/packets.h"
#include "cerver/cerver.h"
#include "cerver/client.h"

#include "cerver/game/lobby.h"

#ifdef PACKETS_DEBUG
#include "cerver/utils/log.h"
#endif

#pragma region protocol

static ProtocolID protocol_id = 0;
static ProtocolVersion protocol_version = { 0, 0 };

ProtocolID packets_get_protocol_id (void) { return protocol_id; }

void packets_set_protocol_id (ProtocolID proto_id) { protocol_id = proto_id; }

ProtocolVersion packets_get_protocol_version (void) { return protocol_version; }

void packets_set_protocol_version (ProtocolVersion version) { protocol_version = version; }

#pragma endregion

#pragma region types

PacketsPerType *packets_per_type_new (void) {

    PacketsPerType *packets_per_type = (PacketsPerType *) malloc (sizeof (PacketsPerType));
    if (packets_per_type) memset (packets_per_type, 0, sizeof (PacketsPerType));
    return packets_per_type;

}

void packets_per_type_delete (void *ptr) { if (ptr) free (ptr); }

void packets_per_type_print (PacketsPerType *packets_per_type) {

    if (packets_per_type) {
        printf ("Error:             %ld\n", packets_per_type->n_error_packets);
        printf ("Auth:              %ld\n", packets_per_type->n_auth_packets);
        printf ("Request:           %ld\n", packets_per_type->n_request_packets);
        printf ("Game:              %ld\n", packets_per_type->n_game_packets);
        printf ("App:               %ld\n", packets_per_type->n_app_packets);
        printf ("App Error:         %ld\n", packets_per_type->n_app_error_packets);
        printf ("Custom:            %ld\n", packets_per_type->n_custom_packets);
        printf ("Test:              %ld\n", packets_per_type->n_test_packets);
        printf ("Unknown:           %ld\n", packets_per_type->n_unknown_packets);
        printf ("Bad:               %ld\n", packets_per_type->n_bad_packets);
    }

}

#pragma endregion

#pragma region header

PacketHeader *packet_header_new (void) {

    PacketHeader *header = (PacketHeader *) malloc (sizeof (PacketHeader));
    if (header) {
        memset (header, 0, sizeof (PacketHeader));
    }

    return header;

}

void packet_header_delete (PacketHeader *header) { if (header) free (header); }

PacketHeader *packet_header_create (PacketType packet_type, size_t packet_size) {

    PacketHeader *header = (PacketHeader *) malloc (sizeof (PacketHeader));
    if (header) {
        memset (header, 0, sizeof (PacketHeader));
        header->protocol_id = protocol_id;
        header->protocol_version = protocol_version;
        header->packet_type = packet_type;
        header->packet_size = packet_size;
    }

    return header;

}

void packet_header_print (PacketHeader *header) {

    if (header) {
        printf ("protocol id: %d\n", header->protocol_id);
        printf ("protocol version: { %d - %d }\n", header->protocol_version.major, header->protocol_version.minor);
        printf ("packet type: %d\n", header->packet_type);
        printf ("packet size: %ld\n", header->packet_size);
    }

}

// allocates space for the dest packet header and copies the data from source
// returns 0 on success, 1 on error
u8 packet_header_copy (PacketHeader **dest, PacketHeader *source) {

    u8 retval = 1;

    if (source) {
        *dest = (PacketHeader *) malloc (sizeof (PacketHeader));
        if (*dest) {
            memcpy (*dest, source, sizeof (PacketHeader));
            retval = 0;
        }
    }

    return retval;

}

#pragma endregion

#pragma region packets

u8 packet_append_data (Packet *packet, void *data, size_t data_size);

Packet *packet_new (void) {

    Packet *packet = (Packet *) malloc (sizeof (Packet));
    if (packet) {
        memset (packet, 0, sizeof (Packet));
        packet->cerver = NULL;
        packet->client = NULL;
        packet->connection = NULL;
        packet->lobby = NULL;

        packet->custom_type = NULL;

        packet->data = NULL;
        packet->data_end = NULL;
        packet->data_ref = false;

        packet->header = NULL;  
        packet->packet = NULL;
        packet->packet_ref = false;
    }

    return packet;

}

// create a new packet with the option to pass values directly
// data is copied into packet buffer and can be safely freed
Packet *packet_create (PacketType type, void *data, size_t data_size) {

    Packet *packet = packet_new ();
    if (packet) {
        packet->packet_type = type;
        if (data) packet_append_data (packet, data, data_size);
    }

    return packet;

}

void packet_delete (void *ptr) {

    if (ptr) {
        Packet *packet = (Packet *) ptr;

        packet->cerver = NULL;
        packet->client = NULL;
        packet->connection = NULL;
        packet->lobby = NULL;

        estring_delete (packet->custom_type);

        if (!packet->data_ref) {
            if (packet->data) free (packet->data);
        }

        packet_header_delete (packet->header);
        if (!packet->packet_ref) {
            if (packet->packet) free (packet->packet);
        }

        free (packet);
    }

}

// sets the pakcet destinatary is directed to and the protocol to use
void packet_set_network_values (Packet *packet, Cerver *cerver, 
    Client *client, Connection *connection, Lobby *lobby) {

    if (packet) {
        packet->cerver = cerver;
        packet->client = client;
        packet->connection = connection;
        packet->lobby = lobby;
    }

}

// sets the data of the packet -> copies the data into the packet
// if the packet had data before it is deleted and replaced with the new one
// returns 0 on success, 1 on error
u8 packet_set_data (Packet *packet, void *data, size_t data_size) {

    u8 retval = 1;

    if (packet && data) {
        // check if there was data in the packet before
        if (!packet->data_ref) {
            if (packet->data) free (packet->data);
        }

        packet->data_size = data_size;
        packet->data = malloc (packet->data_size);
        if (packet->data) {
            memcpy (packet->data, data, data_size);
            packet->data_end = (char *) packet->data;
            packet->data_end += packet->data_size;

            retval = 0;
        }
    }

    return retval;

}

// appends the data to the end if the packet already has data
// if the packet is empty, creates a new buffer
// it creates a new copy of the data and the original can be safely freed
// this does not work if the data has been set using a reference
u8 packet_append_data (Packet *packet, void *data, size_t data_size) {

    u8 retval = 1;

    if (packet && data) {
        // append the data to the end if the packet already has data
        if (packet->data) {
            size_t new_size = packet->data_size + data_size;
            void *new_data = realloc (packet->data, new_size);
            if (new_data) {
                packet->data_end = (char *) new_data;
                packet->data_end += packet->data_size;

                // copy the new buffer
                memcpy (packet->data_end, data, data_size);
                packet->data_end += data_size;

                packet->data = new_data;
                packet->data_size = new_size;

                retval = 0;
            }

            else {
                #ifdef PACKETS_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to realloc packet data!");
                #endif
                packet->data = NULL;
                packet->data_size = 0;
            }
        }

        // if the packet is empty, create a new buffer
        else {
            packet->data_size = data_size;
            packet->data = malloc (packet->data_size);
            if (packet->data) {
                // copy the data to the packet data buffer
                memcpy (packet->data, data, data_size);
                packet->data_end = (char *) packet->data;
                packet->data_end += packet->data_size;

                retval = 0;
            }

            else {
                #ifdef PACKETS_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to allocate packet data!");
                #endif
                packet->data = NULL;
                packet->data_size = 0;
            }
        }
    }

    return retval;

}

// sets a reference to a data buffer to send
// data will not be copied into the packet and will not be freed after use
// this method is usefull for example if you just want to send a raw json packet to a non-cerver
// use this method with packet_send () with the raw flag on
u8 packet_set_data_ref (Packet *packet, void *data, size_t data_size) {
    
    u8 retval = 1;

    if (packet && data) {
        if (!packet->data_ref) {
            if (packet->data) free (packet->data);
        }

        packet->data = data;
        packet->data_size = data_size;
        packet->data_ref = true;

        retval = 0;
    }

    return retval;

}

// sets a the packet's packet using by copying the passed data
// deletes the previuos packet's packet
// returns 0 on succes, 1 on error
u8 packet_set_packet (Packet *packet, void *data, size_t data_size) {

    u8 retval = 1;

    if (packet && data) {
        if (!packet->packet_ref) {
            if (packet->packet) free (packet->packet);
        }

        packet->packet_size = data_size;
        packet->packet = malloc (packet->packet_size);
        if (packet->packet) {
            memcpy (packet->packet, data, data_size);

            retval = 0;
        }
    }

    return retval;

}   

// sets a reference to a data buffer to send as the packet
// data will not be copied into the packet and will not be freed after use
// usefull when you need to generate your own cerver type packet by hand
u8 packet_set_packet_ref (Packet *packet, void *data, size_t packet_size) {

    u8 retval = 1;

    if (packet && data) {
        if (!packet->packet_ref) {
            if (packet->packet) free (packet->packet);
        }

        packet->packet = data;
        packet->packet_size = packet_size;
        packet->packet_ref = true;

        retval = 0;
    }

    return retval;

}

// prepares the packet to be ready to be sent
// returns 0 on sucess, 1 on error
u8 packet_generate (Packet *packet) {

    u8 retval = 0;

    if (packet) {
        if (packet->packet) {
            free (packet->packet);
            packet->packet = NULL;
            packet->packet_size = 0;
        }   

        packet->packet_size = sizeof (PacketHeader) + packet->data_size;
        packet->header = packet_header_create (packet->packet_type, packet->packet_size);

        // create the packet buffer to be sent
        packet->packet = malloc (packet->packet_size);
        if (packet->packet) {
            char *end = (char *) packet->packet;
            memcpy (end, packet->header, sizeof (PacketHeader));

            if (packet->data_size > 0) {
                end += sizeof (PacketHeader);
                memcpy (end, packet->data, packet->data_size);
            }

            retval = 0;
        }
    }

    return retval;

}

// generates a simple request packet of the requested type reday to be sent, 
// and with option to pass some data
Packet *packet_generate_request (PacketType packet_type, u32 req_type, 
    void *data, size_t data_size) {

    Packet *packet = packet_new ();
    if (packet) {
        packet->packet_type = packet_type;

        // generate the request
        packet->data = malloc (sizeof (RequestData));
        ((RequestData *) packet->data)->type = req_type;

        packet->data_size = sizeof (RequestData);
        packet->data_end = (char *) packet->data;
        packet->data_end += sizeof (RequestData);

        // if there is data, append it to the packet data buffer
        if (data) {
            if (packet_append_data (packet, data, data_size)) {
                // we failed to appedn the data into the packet
                packet_delete (packet);
                packet = NULL;
            }
        } 

        // make the packet ready to be sent, and check for errors
        if (packet_generate (packet)) {
            packet_delete (packet);
            packet = NULL;
        }
    }

    return packet;

}

// sends a packet directly using the tcp protocol and the packet sock fd
// returns 0 on success, 1 on error
static u8 packet_send_tcp (const Packet *packet, Connection *connection, int flags, size_t *total_sent, bool raw) {

    u8 retval = 1;

    if (packet && connection) {
        pthread_mutex_lock (connection->socket->write_mutex);

        ssize_t sent = 0;
        char *p = raw ? (char *) packet->data : (char *) packet->packet;
        size_t packet_size = raw ? packet->data_size : packet->packet_size;

        while (packet_size > 0) {
            sent = send (connection->socket->sock_fd, p, packet_size, flags);
            if (sent < 0) {
                break;
            }

            p += sent;
            packet_size -= (size_t) sent;
            retval = 0;
        }

        if (total_sent) *total_sent = (size_t) sent;

        pthread_mutex_unlock (connection->socket->write_mutex);
    }

    return retval;

}

// sends a packet to the socket in two parts, first the header & then the data
// returns 0 on success, 1 on error
static u8 packet_send_split_tcp (const Packet *packet, Connection *connection, int flags, size_t *total_sent) {

    u8 retval = 1;

    if (packet && connection) {
        pthread_mutex_lock (connection->socket->write_mutex);

        size_t actual_sent = 0;

        // first send the header
        bool fail = false;
        ssize_t sent = 0;
        char *p = (char *) packet->header;
        size_t packet_size = sizeof (PacketHeader);

        while (packet_size > 0) {
            sent = send (connection->socket->sock_fd, p, packet_size, flags);
            if (sent < 0) {
                fail = true;
                break;
            }

            p += sent;
            actual_sent += (size_t) sent;
            packet_size -= (size_t) sent;
            fail = false;
        }

        // now send the data
        if (!fail) {
            sent = 0;
            p = (char *) packet->data;
            packet_size = packet->data_size;

            while (packet_size > 0) {
                sent = send (connection->socket->sock_fd, p, packet_size, flags);
                if (sent < 0) break;
                p += sent;
                actual_sent += (size_t) sent;
                packet_size -= (size_t) sent;
            }

            if (total_sent) *total_sent = actual_sent;

            retval = 0;
        }

        pthread_mutex_unlock (connection->socket->write_mutex);
    }

    return retval;

}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static u8 packet_send_udp (const void *packet, size_t packet_size) {

    // TODO:

    return 0;

}
#pragma GCC diagnostic pop

static void packet_send_update_stats (PacketType packet_type, size_t sent,
    Cerver *cerver, Client *client, Connection *connection, Lobby *lobby) {

    if (cerver) {
        cerver->stats->n_packets_sent += 1;
        cerver->stats->total_bytes_sent += sent;
    }

    if (client) {
        client->stats->n_packets_sent += 1;
        client->stats->total_bytes_sent += sent;
    }

    connection->stats->n_packets_sent += 1;
    connection->stats->total_bytes_sent += sent; 

    if (lobby) {
        lobby->stats->n_packets_sent += 1;
        lobby->stats->bytes_sent += sent;
    }

    switch (packet_type) {
        case CERVER_PACKET: break;

        case CLIENT_PACKET: break;

        case ERROR_PACKET: 
            if (cerver) cerver->stats->sent_packets->n_error_packets += 1;
            if (client) client->stats->sent_packets->n_error_packets += 1;
            connection->stats->sent_packets->n_error_packets += 1;
            if (lobby) lobby->stats->sent_packets->n_error_packets += 1;
            break;

        case AUTH_PACKET: 
            if (cerver) cerver->stats->sent_packets->n_auth_packets += 1;
            if (client) client->stats->sent_packets->n_auth_packets += 1;
            connection->stats->sent_packets->n_auth_packets += 1;
            if (lobby) lobby->stats->sent_packets->n_auth_packets += 1;
            break;

        case REQUEST_PACKET: 
            if (cerver) cerver->stats->sent_packets->n_request_packets += 1;
            if (client) client->stats->sent_packets->n_request_packets += 1;
            connection->stats->sent_packets->n_request_packets += 1;
            if (lobby) lobby->stats->sent_packets->n_request_packets += 1;
            break;

        case GAME_PACKET:
            if (cerver) cerver->stats->sent_packets->n_game_packets += 1; 
            if (client) client->stats->sent_packets->n_game_packets += 1;
            connection->stats->sent_packets->n_game_packets += 1;
            if (lobby) lobby->stats->sent_packets->n_game_packets += 1;
            break;

        case APP_PACKET:
            if (cerver) cerver->stats->sent_packets->n_app_packets += 1;
            if (client) client->stats->sent_packets->n_app_packets += 1;
            connection->stats->sent_packets->n_app_packets += 1;
            if (lobby) lobby->stats->sent_packets->n_app_packets += 1;
            break;

        case APP_ERROR_PACKET: 
            if (cerver) cerver->stats->sent_packets->n_app_error_packets += 1;
            if (client) client->stats->sent_packets->n_app_error_packets += 1;
            connection->stats->sent_packets->n_app_error_packets += 1;
            if (lobby) lobby->stats->sent_packets->n_app_error_packets += 1;
            break;

        case CUSTOM_PACKET:
            if (cerver) cerver->stats->sent_packets->n_custom_packets += 1;
            if (client) client->stats->sent_packets->n_custom_packets += 1;
            connection->stats->sent_packets->n_custom_packets += 1;
            if (lobby) lobby->stats->sent_packets->n_custom_packets += 1;
            break;

        case TEST_PACKET: 
            if (cerver) cerver->stats->sent_packets->n_test_packets += 1;
            if (client) client->stats->sent_packets->n_test_packets += 1;
            connection->stats->sent_packets->n_test_packets += 1;
            if (lobby) lobby->stats->sent_packets->n_test_packets += 1;
            break;

        case DONT_CHECK_TYPE:
        default: break;
    }

}

static inline u8 packet_send_internal (const Packet *packet, int flags, size_t *total_sent, 
    bool raw, bool split,
    Cerver *cerver, Client *client, Connection *connection, Lobby *lobby) {

    u8 retval = 1;

    if (packet && connection) {
        switch (connection->protocol) {
            case PROTOCOL_TCP: {
                size_t sent = 0;

                if (!(split ? packet_send_split_tcp (packet, connection, flags, &sent)
                    : packet_send_tcp (packet, connection, flags, &sent, raw))) {
                    if (total_sent) *total_sent = sent;

                    packet_send_update_stats (
                        packet->packet_type, sent,
                        cerver, client, connection, lobby
                    );

                    retval = 0;
                }

                else {
                    #ifdef PACKETS_DEBUG
                    printf ("\n");
                    perror ("Error");
                    printf ("\n");
                    #endif

                    if (cerver) cerver->stats->sent_packets->n_bad_packets += 1;
                    if (client) client->stats->sent_packets->n_bad_packets += 1;
                    if (connection) connection->stats->sent_packets->n_bad_packets += 1;

                    if (total_sent) *total_sent = 0;
                }
            } break;

            case PROTOCOL_UDP:
                break;

            default: break;
        }
    }

    return retval;

}

// sends a packet using its network values
// raw flag to send a raw packet (only the data that was set to the packet, without any header)
// returns 0 on success, 1 on error
u8 packet_send (const Packet *packet, int flags, size_t *total_sent, bool raw) {

    return packet_send_internal (
        packet, flags, total_sent, 
        raw, false,
        packet->cerver, packet->client, packet->connection, packet->lobby
    );

}

// sends a packet to the specified destination
// sets flags to 0
// at least a packet & an active connection are required for this method to succeed
// raw flag to send a raw packet (only the data that was set to the packet, without any header)
// returns 0 on success, 1 on error
u8 packet_send_to (const Packet *packet, size_t *total_sent, bool raw,
    Cerver *cerver, Client *client, Connection *connection, Lobby *lobby) {

    return packet_send_internal (
        packet, 0, total_sent, 
        raw, false,
        cerver, client, connection, lobby
    );

}

// sends a packet to the socket in two parts, first the header & then the data
// this method can be useful when trying to forward a big received packet without the overhead of 
// performing and additional copy to create a continuos data (packet) buffer
// the socket's write mutex will be locked to ensure that the packet
// is sent correctly and to avoid race conditions
// returns 0 on success, 1 on error
u8 packet_send_split (const Packet *packet, int flags, size_t *total_sent) {

    return packet_send_internal (
        packet, flags, total_sent, 
        false, true,
        packet->cerver, packet->client, packet->connection, packet->lobby
    );

}

// sends a packet to the socket in two parts, first the header & then the data
// works just as packet_send_split () but with the flags set to 0
// returns 0 on success, 1 on error
u8 packet_send_to_split (const Packet *packet, size_t *total_sent,
    Cerver *cerver, Client *client, Connection *connection, Lobby *lobby) {

    return packet_send_internal (
        packet, 0, total_sent, 
        false, true,
        cerver, client, connection, lobby
    );

}

// sends a packet directly to the socket
// raw flag to send a raw packet (only the data that was set to the packet, without any header)
// returns 0 on success, 1 on error
u8 packet_send_to_socket (const Packet *packet, Socket *socket, 
    int flags, size_t *total_sent, bool raw) {

    u8 retval = 0;

    if (packet) {
        ssize_t sent = 0;
        const char *p = raw ? (char *) packet->data : (char *) packet->packet;
        size_t packet_size = raw ? packet->data_size : packet->packet_size;

        pthread_mutex_lock (socket->write_mutex);

        while (packet_size > 0) {
            sent = send (socket->sock_fd, p, packet_size, flags);
            if (sent < 0) {
                retval = 1;
                break;
            };

            p += sent;
            packet_size -= sent;
        }

        if (total_sent) *total_sent = (size_t) sent;

        pthread_mutex_lock (socket->write_mutex);
    }

    return retval;

}

// check if packet has a compatible protocol id and a version
// returns false on a bad packet
bool packet_check (Packet *packet) {

    bool retval = false;

    if (packet) {
        PacketHeader *header = packet->header;

        if (header->protocol_id == protocol_id) {
            if ((header->protocol_version.major <= protocol_version.major)
                && (header->protocol_version.minor >= protocol_version.minor)) {
                retval = true;
            }

            else {
                #ifdef PACKETS_DEBUG
                cerver_log_msg (stdout, LOG_WARNING, LOG_PACKET, "Packet with incompatible version.");
                #endif
            }
        }

        else {
            #ifdef PACKETS_DEBUG
            cerver_log_msg (stdout, LOG_WARNING, LOG_PACKET, "Packet with unknown protocol ID.");
            #endif
        }
    }

    return retval;

}

#pragma endregion