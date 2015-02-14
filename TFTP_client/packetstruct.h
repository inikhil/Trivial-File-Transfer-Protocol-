struct Request {
    uint16_t opcode;
    char *filename;
    uint8_t e1;
    char *mode;
    uint8_t e2;
};

struct Data{
	uint16_t opcode;
	uint16_t block;
	char *data;
};

struct Ack{
	uint16_t opcode;
	uint16_t block;
};

struct Error{
	uint16_t opcode;
	uint16_t ErrorCode;
	char *errmsg;
	uint8_t e1;
};