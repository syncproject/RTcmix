class MIX : public Instrument {
	int outchan[8];
	float amp,*amptable,tabs[2], *in;
	int skip;

public:
	MIX();
	virtual ~MIX();
	int init(float*, short);
	int run();
	};
