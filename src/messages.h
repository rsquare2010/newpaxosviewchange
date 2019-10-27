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


void serializeVC(ViewChange* viewChange, char* messageSerialized) {
    uint32_t temp;
    temp = htonl(viewChange->type);
    memcpy(&messageSerialized[0], &temp, 4);
    temp = htonl(viewChange->server_id);
    memcpy(&messageSerialized[4], &temp, 4);
    temp = htonl(viewChange->attempted);
    memcpy(&messageSerialized[8], &temp, 4);
}

void serializeVP(VCProof* vcProof, char* messageSerialized) {
    uint32_t temp;
    temp = htonl(vcProof->type);
    memcpy(&messageSerialized[0], &temp, 4);
    temp = htonl(vcProof->server_id);
    memcpy(&messageSerialized[4], &temp, 4);
    temp = htonl(vcProof->installed);
    memcpy(&messageSerialized[8], &temp, 4);
}

void deserializeVC(char* buffer, ViewChange* viewChange)
{
    uint32_t tempo;
    memcpy(&tempo, &buffer[0], 4);
    viewChange->type = ntohl(tempo);
    memcpy(&tempo, &buffer[4], 4);
    viewChange->server_id = ntohl(tempo);
    memcpy(&tempo, &buffer[8], 4);
    viewChange->attempted = ntohl(tempo);
}

void deserializeVP(char* buffer, VCProof* vcProof)
{
    uint32_t tempo;
    memcpy(&tempo, &buffer[0], 4);
    vcProof->type = ntohl(tempo);
    memcpy(&tempo, &buffer[4], 4);
    vcProof->server_id = ntohl(tempo);
    memcpy(&tempo, &buffer[8], 4);
    vcProof->installed = ntohl(tempo);
}