struct __attribute__((packed)) FFF_ID
{
	uint16_t      major,
	              minor;
};

struct __attribute__((packed)) FFF_ENVP_HEADER
{
	struct FFF_ID       id;
	uint8_t             num_envelopes;
	uint8_t             flags;
	uint8_t             mode;
	uint8_t             index_type;
/*	struct FFF_ENVP_ENTRY *records;*/
};

struct __attribute__((packed)) FFF_ENVP_ENTRY
{
	uint16_t     nattack;
	uint16_t     nrelease;
	uint16_t     sustain_offset;
	uint16_t     sustain_rate;
	uint16_t     release_rate;
	uint8_t      hirange;
	uint8_t      pad;
};

struct __attribute__((packed)) FFF_ENVP_POINT
{
	uint16_t            next;

	union
	{
	        uint16_t    rate;
	        uint16_t    time;
	} data;
};

struct __attribute__((packed)) FFF_DATA_HEADER
{
	struct FFF_ID       id;
	char filename[128];         /* dynamic length */
};

struct __attribute__((packed)) FFF_PROG_HEADER
{
	struct FFF_ID id;
	struct FFF_ID version;
};

struct __attribute__((packed)) FFF_PTCH_HEADER
{
	struct FFF_ID  id;
	uint16_t       nlayers;
	uint8_t        layer_mode;
	uint8_t        excl_mode;
	uint16_t       excl_group;
	uint8_t        effect1;
	uint8_t        effect1_depth;
	uint8_t        effect2;
	uint8_t        effect2_depth;
	uint8_t        bank;
	uint8_t        program;
//	struct FFF_LAYR_CHUNK *iw_layer;
};

struct __attribute__((packed)) FFF_LFO
{
	uint16_t    freq;
	int16_t     depth;
	int16_t     sweep;
	uint8_t     shape;
	uint8_t     delay;
};

struct __attribute__((packed)) FFF_LAYR_HEADER
{
	struct FFF_ID          id;
	uint8_t                nwaves;
	uint8_t                flags;
	uint8_t                high_range;
	uint8_t                low_range;
	uint8_t                pan;
	uint8_t                pan_freq_scale;
	struct FFF_LFO         tremolo;
	struct FFF_LFO         vibrato;
	uint8_t                velocity_mode;
	uint8_t                attenuation;
	int16_t                freq_scale;
	uint8_t                freq_center;
	uint8_t                layer_event;
	struct FFF_ID          penv;
	struct FFF_ID          venv;
//	struct FFF_WAVE_CHUNK *waves;
};

struct __attribute__((packed)) FFF_WAVE_HEADER
{
	struct FFF_ID         id;
	uint32_t              size;
	uint32_t              start; /* offset */
	uint32_t              loopstart; /* last 4 bit are used for fraction */
	uint32_t              loopend;   /* last 4 bit are used for fraction */
	uint32_t              m_start; /* ? */
	uint32_t              sample_ratio; /* GIPC.CPP:(dword)(45158.4L / gusWave->sample_rate * gusWave->root_frequency); */
	uint8_t               attenuation;
	uint8_t               low_note;
	uint8_t               high_note;
	uint8_t               format; /* 0x01 = 8bit, 0x02 = signed, 0x04 = loop forward, 0x08 = loop enabled, 0x10 = bi-direction loop, 0x20 = uLaw */
	uint8_t               m_format; /* ? */
	struct FFF_ID         data;
};


static inline void FFF_ID_endian(struct FFF_ID *a)
{
	a->major = uint16_little (a->major);
	a->minor = uint16_little (a->minor);
}


static inline void FFF_ENVP_HEADER_endian(struct FFF_ENVP_HEADER *a)
{
	FFF_ID_endian (&a->id);
}


static inline void FFF_ENVP_ENTRY_endian(struct FFF_ENVP_ENTRY *a)
{
	a->nattack = uint16_little (a->nattack);
	a->nrelease = uint16_little (a->nrelease);
	a->sustain_offset = uint16_little (a->sustain_offset);
	a->sustain_rate = uint16_little (a->sustain_rate);
	a->release_rate = uint16_little (a->release_rate);
}


static inline void FFF_ENVP_POINT_endian(struct FFF_ENVP_POINT *a)
{
	a->next = uint16_little (a->next);
	a->data.rate = uint16_little (a->data.rate);
}

static inline void FFF_DATA_HEADER_endian(struct FFF_DATA_HEADER *a)
{
	FFF_ID_endian (&a->id);
}

static inline void FFF_PROG_HEADER_endian(struct FFF_PROG_HEADER *a)
{
	FFF_ID_endian (&a->id);
	FFF_ID_endian (&a->version);
}

static inline void FFF_PTCH_HEADER_endian(struct FFF_PTCH_HEADER *a)
{
	FFF_ID_endian (&a->id);
	a->nlayers = uint16_little (a->nlayers);
	a->excl_group = uint16_little (a->excl_group);	
}

static inline void FFF_LFO_endian(struct FFF_LFO *a)
{
	a->freq = uint16_little (a->freq);
	a->depth = int16_little (a->depth);
	a->sweep = int16_little (a->sweep);
}

static inline void FFF_LAYR_HEADER_endian(struct FFF_LAYR_HEADER *a)
{
	FFF_ID_endian (&a->id);
	FFF_LFO_endian (&a->tremolo);
	FFF_LFO_endian (&a->vibrato);
	a->freq_scale = int16_little (a->freq_scale);
}

static inline void FFF_WAVE_HEADER_endian(struct FFF_WAVE_HEADER *a)
{
	FFF_ID_endian (&a->id);
	a->size = int32_little (a->size);
	a->start = int32_little (a->start);
	a->loopstart = int32_little (a->loopstart);
	a->loopend = int32_little (a->loopend);
	a->m_start = int32_little (a->m_start);
	a->sample_ratio = int32_little (a->sample_ratio);
	FFF_ID_endian (&a->data);
}
