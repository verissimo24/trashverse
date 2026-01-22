#ifndef COMM_H
#define COMM_H

#include <zmq.h>   
#include <stddef.h>
#include <stdint.h>

#include "direction.h"
#include "universe-data.h"

struct UniverseState;
struct UniverseStats;


typedef struct {
    void *ctx;
    void *sock;
}zmqChannel;


/**
 * @brief Create and bind the server ZeroMQ REP channel.
 *
 * Creates a new ZeroMQ context and a REP (reply) socket, then binds the socket
 * to a fixed TCP endpoint so the server can accept client
 * requests using the REQ/REP pattern. On failure, the function prints an error
 * message and returns a channel with NULL fields.
 *
 * @param void
 * @return zmqChannel structure containing the initialized context and socket on success;
 *         on failure, returns a channel with ctx and/or sock set to NULL.
 */
zmqChannel create_server_channel(int reqrep_port);


/**
 * @brief Create and connect the client-side ZeroMQ REQ channel.
 *
 * Creates a new ZeroMQ context and a REQ (request) socket, then connects the socket
 * to the server endpoint so the client can issue requests
 * using the REQ/REP pattern. On failure, the function prints an error message and
 * returns a channel with NULL fields.
 *
 * @param address Server hostname or IP address (without port).
 * @return zmqChannel structure containing the initialized context and socket on success;
 *         on failure, returns a channel with ctx and/or sock set to NULL.
 */
zmqChannel create_client_channel(const char *address, int reqrep_port);


/**
 * @brief Send a CONNECT request message to the server (REQ/REP protocol).
 *
 * Builds a protobuf ClientMessage of type CONNECT, serializes it into a byte
 * buffer, and sends it through the given ZeroMQ socket. This is typically the
 * first message sent by a client to request a ship ID and authentication token.
 *
 * @param fd ZeroMQ socket (REQ) used to communicate with the server.
 */
void send_connection_message(void * fd); 


/**
 * @brief Send a MOVE request message to the server for a given ship.
 *
 * Builds a protobuf ClientMessage of type MOVE containing the ship ID,
 * the desired direction (converted to the protobuf enum), and the client's
 * authentication token. The message is serialized into a byte buffer and sent
 * through the given ZeroMQ socket using the REQ/REP pattern.
 *
 * @param fd ZeroMQ socket (REQ) used to communicate with the server.
 * @param ch Ship identifier character.
 * @param d Movement direction (UP/DOWN/LEFT/RIGHT).
 * @param token Authentication token associated with the ship/client.
 */
void send_movement_message(void * fd, char ch, direction_t direction, uint32_t token);



void send_quit_message(void *fd, char ch, uint32_t token);


/**
 * @brief Receive and decode a client request message (CONNECT or MOVE).
 *
 * Receives a serialized protobuf ClientMessage from the given ZeroMQ socket,
 * unpacks it, and fills the provided output parameters with the decoded content.
 * The function identifies whether the message is a CONNECT or MOVE request:
 *   - For CONNECT: sets message_type to "CONNECT".
 *   - For MOVE: sets message_type to "MOVE" and extracts ship_id (first char),
 *     direction (converted from protobuf enum), and the authentication token.
 *
 * @param fd ZeroMQ socket.
 * @param message_type Output string set to "CONNECT", "MOVE", or empty on invalid message.
 * @param c Output ship identifier character (only meaningful for MOVE).
 * @param d Output movement direction (only meaningful for MOVE).
 * @param token Output authentication token (only meaningful for MOVE).
 */
void read_message(void * fd, char * message_type, char * c, direction_t *d, uint32_t *token);


/**
 * @brief Send a generic ACK response to the client (OK or error message).
 *
 * Builds a protobuf ServerAck message where the ok flag is set to true only when
 * the provided message equals "OK". The ACK is serialized and sent over the given 
 * ZeroMQ socket as the reply in the REQ/REP exchange.
 *
 * @param fd ZeroMQ socket (typically a REP socket on the server).
 * @param message Status string to send ("OK" or "ERROR").
 */
void send_response(void * fd, char * message);


/**
 * @brief Send a CONNECT_ACK response containing the assigned ship ID and token.
 *
 * Builds a protobuf ServerAck message indicating success (ok=1) and includes the
 * ship identifier assigned by the server as well as the authentication token that
 * the client must attach to subsequent MOVE requests. The message is serialized and
 * sent over the given ZeroMQ socket as the reply in the REQ/REP exchange.
 *
 * @param fd ZeroMQ socket.
 * @param ship_id Ship identifier assigned to the client.
 * @param token Authentication token assigned to the client for future requests.
 */
void send_connect_response(void *fd, char ship_id, uint32_t token);


/**
 * @brief Receive and decode a generic server ACK response into a string.
 *
 * Receives a serialized protobuf ServerAck message from the given ZeroMQ socket,
 * unpacks it, and extracts a status string into the provided buffer.
 * If the ACK contains a text message (ack->msg), that text is copied into
 * @p message. Otherwise, a fallback string is generated based on ack->ok ("OK" on
 * success, "ERROR" on failure). On receive or unpack errors, @p message is set to
 * an empty string.
 *
 * @param fd ZeroMQ socket (REQ).
 * @param message Output buffer to store the extracted status string.
 */
void receive_response(void * fd, char * message);


/**
 * @brief Receive and validate the server's CONNECT acknowledgment (ship ID + token).
 *
 * Receives a serialized protobuf ServerAck message from the given ZeroMQ socket,
 * unpacks it, and validates that it represents a successful CONNECT reply,
 * including a ship_id and a token. On success, it extracts the assigned ship
 * identifier and the token into the provided output parameters.
 *
 * @param fd ZeroMQ socket (REQ) used to communicate with the server.
 * @param ship_id_out Output pointer where the assigned ship ID character is stored.
 * @param token_out Output pointer where the assigned authentication token is stored.
 * @return 0 on success; -1 on failure.
 */
int receive_connect_ack(void *fd, char *ship_id_out, uint32_t *token_out);


zmqChannel create_server_pub_channel(int pub_port);
zmqChannel create_client_sub_channel(const char *address, int pub_port, const char *topic);

int comm_pub_send_universe_state(void *pub_sock, const Universe *u, int game_over);
struct UniverseState *comm_sub_recv_universe_state(void *sub_sock);

int comm_pub_send_universe_stats(void *pub_sock, const Universe *u, int server_quit);
struct UniverseStats *comm_sub_recv_universe_stats(void *sub_sock);



/**
 * @brief Close a ZeroMQ channel and release its resources.
 *
 * Closes the socket and shuts down the associated ZeroMQ context,
 * resetting the stored pointers to NULL. 
 *
 * @param ch Pointer to the zmqChannel structure to destroy.
 */
void destroy_channel(zmqChannel *ch);   

#endif 
