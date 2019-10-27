typedef struct {
    uint32_t type;
    uint32_t server_id;
    uint32_t attempted;
} ViewChange;

typedef struct {
    uint32_t type;
    uint32_t server_id;
    uint32_t installed;
} VCProof;


void serializeVC(ViewChange* sendMessage, char* messageSerialized) {
    uint32_t temp;
    temp = htonl(sendMessage->type);
    memcpy(&messageSerialized[0], &temp, 4);
    temp = htonl(sendMessage->server_id);
    memcpy(&messageSerialized[4], &temp, 4);
    temp = htonl(sendMessage->attempted);
    memcpy(&messageSerialized[8], &temp, 4);
}

void serializeVP(VCProof* ackMessage, char* messageSerialized) {
    uint32_t temp;
    temp = htonl(sendMessage->type);
    memcpy(&messageSerialized[0], &temp, 4);
    temp = htonl(sendMessage->server_id);
    memcpy(&messageSerialized[4], &temp, 4);
    temp = htonl(sendMessage->installed);
    memcpy(&messageSerialized[8], &temp, 4);
}

void deserializeVC(char* buffer, ViewChange* dataMessage)
{
    uint32_t tempo;
    memcpy(&tempo, &buffer[0], 4);
    dataMessage->type = ntohl(tempo);
    memcpy(&tempo, &buffer[4], 4);
    dataMessage->server_id = ntohl(tempo);
    memcpy(&tempo, &buffer[8], 4);
    dataMessage->attempted = ntohl(tempo);
}

void deserializeVP(char* buffer, VCProof* ackMessage)
{
    uint32_t tempo;
    memcpy(&tempo, &buffer[0], 4);
    dataMessage->type = ntohl(tempo);
    memcpy(&tempo, &buffer[4], 4);
    dataMessage->server_id = ntohl(tempo);
    memcpy(&tempo, &buffer[8], 4);
    dataMessage->installed = ntohl(tempo);
}