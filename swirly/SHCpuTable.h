
// function pointer table (interpretative/dynamic) 

typedef struct
{
    void (SHCpu::*opcode_handler)(Word);   /* handler function */
    Word mask;                             /* mask on opcode */
    Word match;                            /* what to match after masking */
} opcode_handler_struct;

static opcode_handler_struct sh_opcode_handler_table[] = {

    /*   function                   mask    match */
    {&SHCpu::ADDI                , 0xf000, 0x7000},
    {&SHCpu::BRA                 , 0xf000, 0xa000},
    {&SHCpu::BSR                 , 0xf000, 0xb000},
    {&SHCpu::MOVI                , 0xf000, 0xe000},
    {&SHCpu::MOVWI               , 0xf000, 0x9000},
    {&SHCpu::MOVLI               , 0xf000, 0xd000},
    {&SHCpu::MOVLS4              , 0xf000, 0x1000},
    {&SHCpu::MOVLL4              , 0xf000, 0x5000},
    {&SHCpu::LDCRBANK            , 0xf08f, 0x408e},
    {&SHCpu::LDCMRBANK           , 0xf08f, 0x4087},
    {&SHCpu::STCRBANK            , 0xf08f, 0x0082},
    {&SHCpu::STCMRBANK           , 0xf08f, 0x4083},
    {&SHCpu::FADD                , 0xf00f, 0xf000},
    {&SHCpu::FMUL                , 0xf00f, 0xf002},
    {&SHCpu::FMOV                , 0xf00f, 0xf00c},
    {&SHCpu::FMOV_STORE          , 0xf00f, 0xf00a},
    {&SHCpu::FMOV_LOAD           , 0xf00f, 0xf008},
    {&SHCpu::FMOV_RESTORE        , 0xf00f, 0xf009},
    {&SHCpu::FMOV_SAVE           , 0xf00f, 0xf00b},
    {&SHCpu::FMOV_INDEX_LOAD     , 0xf00f, 0xf006},
    {&SHCpu::FMOV_INDEX_STORE    , 0xf00f, 0xf007},
    {&SHCpu::FCMPEQ              , 0xf00f, 0xf004},
    {&SHCpu::FCMPGT              , 0xf00f, 0xf005},
    {&SHCpu::FDIV                , 0xf00f, 0xf003},
    {&SHCpu::FMAC                , 0xf00f, 0xf00e},
    {&SHCpu::FSUB                , 0xf00f, 0xf001},
    {&SHCpu::ADD                 , 0xf00f, 0x300c},
    {&SHCpu::ADDC                , 0xf00f, 0x300e},
    {&SHCpu::ADDV                , 0xf00f, 0x300f},
    {&SHCpu::AND                 , 0xf00f, 0x2009},
    {&SHCpu::CMPEQ               , 0xf00f, 0x3000},
    {&SHCpu::CMPGE               , 0xf00f, 0x3003},
    {&SHCpu::CMPGT               , 0xf00f, 0x3007},
    {&SHCpu::CMPHI               , 0xf00f, 0x3006},
    {&SHCpu::CMPHS               , 0xf00f, 0x3002},
    {&SHCpu::CMPSTR              , 0xf00f, 0x200c},
    {&SHCpu::DIV0S               , 0xf00f, 0x2007},
    {&SHCpu::DIV1                , 0xf00f, 0x3004},
    {&SHCpu::DMULS               , 0xf00f, 0x300d},
    {&SHCpu::DMULU               , 0xf00f, 0x3005},
    {&SHCpu::EXTSB               , 0xf00f, 0x600e},
    {&SHCpu::EXTSW               , 0xf00f, 0x600f},
    {&SHCpu::EXTUB               , 0xf00f, 0x600c},
    {&SHCpu::EXTUW               , 0xf00f, 0x600d},
    {&SHCpu::DO_MACL             , 0xf00f, 0x000f},
    {&SHCpu::MACW                , 0xf00f, 0x400f},
    {&SHCpu::MOV                 , 0xf00f, 0x6003},
    {&SHCpu::MOVBS               , 0xf00f, 0x2000},
    {&SHCpu::MOVWS               , 0xf00f, 0x2001},
    {&SHCpu::MOVLS               , 0xf00f, 0x2002},
    {&SHCpu::MOVBL               , 0xf00f, 0x6000},
    {&SHCpu::MOVWL               , 0xf00f, 0x6001},
    {&SHCpu::MOVLL               , 0xf00f, 0x6002},
    {&SHCpu::MOVBM               , 0xf00f, 0x2004},
    {&SHCpu::MOVWM               , 0xf00f, 0x2005},
    {&SHCpu::MOVLM               , 0xf00f, 0x2006},
    {&SHCpu::MOVBP               , 0xf00f, 0x6004},
    {&SHCpu::MOVWP               , 0xf00f, 0x6005},
    {&SHCpu::MOVLP               , 0xf00f, 0x6006},
    {&SHCpu::MOVBS0              , 0xf00f, 0x0004},
    {&SHCpu::MOVWS0              , 0xf00f, 0x0005},
    {&SHCpu::MOVLS0              , 0xf00f, 0x0006},
    {&SHCpu::MOVBL0              , 0xf00f, 0x000c},
    {&SHCpu::MOVWL0              , 0xf00f, 0x000d},
    {&SHCpu::MOVLL0              , 0xf00f, 0x000e},
    {&SHCpu::MULL                , 0xf00f, 0x0007},
    {&SHCpu::MULS                , 0xf00f, 0x200f},
    {&SHCpu::MULU                , 0xf00f, 0x200e},
    {&SHCpu::NEGC                , 0xf00f, 0x600a},
    {&SHCpu::NEG                 , 0xf00f, 0x600b},
    {&SHCpu::NOT                 , 0xf00f, 0x6007},
    {&SHCpu::OR                  , 0xf00f, 0x200b},
    {&SHCpu::SHAD                , 0xf00f, 0x400c},
    {&SHCpu::SHLD                , 0xf00f, 0x400d},
    {&SHCpu::SUB                 , 0xf00f, 0x3008},
    {&SHCpu::SUBC                , 0xf00f, 0x300a},
    {&SHCpu::SUBV                , 0xf00f, 0x300b},
    {&SHCpu::SWAPB               , 0xf00f, 0x6008},
    {&SHCpu::SWAPW               , 0xf00f, 0x6009},
    {&SHCpu::TST                 , 0xf00f, 0x2008},
    {&SHCpu::XOR                 , 0xf00f, 0x200a},
    {&SHCpu::XTRCT               , 0xf00f, 0x200d},
    {&SHCpu::MOVBS4              , 0xff00, 0x8000},
    {&SHCpu::MOVWS4              , 0xff00, 0x8100},
    {&SHCpu::MOVBL4              , 0xff00, 0x8400},
    {&SHCpu::MOVWL4              , 0xff00, 0x8500},
    {&SHCpu::ANDI                , 0xff00, 0xc900},
    {&SHCpu::ANDM                , 0xff00, 0xcd00},
    {&SHCpu::BF                  , 0xff00, 0x8b00},
    {&SHCpu::BFS                 , 0xff00, 0x8f00},
    {&SHCpu::BT                  , 0xff00, 0x8900},
    {&SHCpu::BTS                 , 0xff00, 0x8d00},
    {&SHCpu::CMPIM               , 0xff00, 0x8800},
    {&SHCpu::MOVBLG              , 0xff00, 0xc400},
    {&SHCpu::MOVWLG              , 0xff00, 0xc500},
    {&SHCpu::MOVLLG              , 0xff00, 0xc600},
    {&SHCpu::MOVBSG              , 0xff00, 0xc000},
    {&SHCpu::MOVWSG              , 0xff00, 0xc100},
    {&SHCpu::MOVLSG              , 0xff00, 0xc200},
    {&SHCpu::MOVA                , 0xff00, 0xc700},
    {&SHCpu::ORI                 , 0xff00, 0xcb00},
    {&SHCpu::ORM                 , 0xff00, 0xcf00},
    {&SHCpu::TRAPA               , 0xff00, 0xc300},
    {&SHCpu::TSTI                , 0xff00, 0xc800},
    {&SHCpu::TSTM                , 0xff00, 0xcc00},
    {&SHCpu::XORI                , 0xff00, 0xca00},
    {&SHCpu::XORM                , 0xff00, 0xce00},
    {&SHCpu::BRAF                , 0xf0ff, 0x0023},
    {&SHCpu::BSRF                , 0xf0ff, 0x0003},
    {&SHCpu::CMPPL               , 0xf0ff, 0x4015},
    {&SHCpu::CMPPZ               , 0xf0ff, 0x4011},
    {&SHCpu::DT                  , 0xf0ff, 0x4010},
    {&SHCpu::JSR                 , 0xf0ff, 0x400b},
    {&SHCpu::JMP                 , 0xf0ff, 0x402b},
    {&SHCpu::LDCSR               , 0xf0ff, 0x400e},
    {&SHCpu::LDCGBR              , 0xf0ff, 0x401e},
    {&SHCpu::LDCVBR              , 0xf0ff, 0x402e},
    {&SHCpu::LDCSSR              , 0xf0ff, 0x403e},
    {&SHCpu::LDCSPC              , 0xf0ff, 0x404e},
    {&SHCpu::LDCDBR              , 0xf0ff, 0x40fa},
    {&SHCpu::LDCMSR              , 0xf0ff, 0x4007},
    {&SHCpu::LDCMGBR             , 0xf0ff, 0x4017},
    {&SHCpu::LDCMVBR             , 0xf0ff, 0x4027},
    {&SHCpu::LDCMSSR             , 0xf0ff, 0x4037},
    {&SHCpu::LDCMSPC             , 0xf0ff, 0x4047},
    {&SHCpu::LDCMDBR             , 0xf0ff, 0x40f6},
    {&SHCpu::LDSMACH             , 0xf0ff, 0x400a},
    {&SHCpu::LDSMACL             , 0xf0ff, 0x401a},
    {&SHCpu::LDSPR               , 0xf0ff, 0x402a},
    {&SHCpu::LDSFPSCR            , 0xf0ff, 0x406a},
    {&SHCpu::LDSFPUL             , 0xf0ff, 0x405a},
    {&SHCpu::LDSMFPUL            , 0xf0ff, 0x4056},
    {&SHCpu::LDSMFPSCR           , 0xf0ff, 0x4066},
    {&SHCpu::LDSMMACH            , 0xf0ff, 0x4006},
    {&SHCpu::LDSMMACL            , 0xf0ff, 0x4016},
    {&SHCpu::LDSMPR              , 0xf0ff, 0x4026},
    {&SHCpu::MOVCAL              , 0xf0ff, 0x00c3},
    {&SHCpu::MOVT                , 0xf0ff, 0x0029},
    {&SHCpu::OCBI                , 0xf0ff, 0x0093},
    {&SHCpu::OCBP                , 0xf0ff, 0x00a3},
    {&SHCpu::OCBWB               , 0xf0ff, 0x00b3},
    {&SHCpu::PREF                , 0xf0ff, 0x0083},
    {&SHCpu::ROTCL               , 0xf0ff, 0x4024},
    {&SHCpu::ROTCR               , 0xf0ff, 0x4025},
    {&SHCpu::ROTL                , 0xf0ff, 0x4004},
    {&SHCpu::ROTR                , 0xf0ff, 0x4005},
    {&SHCpu::SHAL                , 0xf0ff, 0x4020},
    {&SHCpu::SHAR                , 0xf0ff, 0x4021},
    {&SHCpu::SHLL                , 0xf0ff, 0x4000},
    {&SHCpu::SHLL2               , 0xf0ff, 0x4008},
    {&SHCpu::SHLL8               , 0xf0ff, 0x4018},
    {&SHCpu::SHLL16              , 0xf0ff, 0x4028},
    {&SHCpu::SHLR                , 0xf0ff, 0x4001},
    {&SHCpu::SHLR2               , 0xf0ff, 0x4009},
    {&SHCpu::SHLR8               , 0xf0ff, 0x4019},
    {&SHCpu::SHLR16              , 0xf0ff, 0x4029},
    {&SHCpu::STCSR               , 0xf0ff, 0x0002},
    {&SHCpu::STCGBR              , 0xf0ff, 0x0012},
    {&SHCpu::STCVBR              , 0xf0ff, 0x0022},
    {&SHCpu::STCSSR              , 0xf0ff, 0x0032},
    {&SHCpu::STCSPC              , 0xf0ff, 0x0042},
    {&SHCpu::STCSGR              , 0xf0ff, 0x003a},
    {&SHCpu::STCDBR              , 0xf0ff, 0x00fa},
    {&SHCpu::STCMSR              , 0xf0ff, 0x4003},
    {&SHCpu::STCMGBR             , 0xf0ff, 0x4013},
    {&SHCpu::STCMVBR             , 0xf0ff, 0x4023},
    {&SHCpu::STCMSSR             , 0xf0ff, 0x4033},
    {&SHCpu::STCMSPC             , 0xf0ff, 0x4043},
    {&SHCpu::STCMSGR             , 0xf0ff, 0x4032},
    {&SHCpu::STCMDBR             , 0xf0ff, 0x40f2},
    {&SHCpu::STSMACH             , 0xf0ff, 0x000a},
    {&SHCpu::STSMACL             , 0xf0ff, 0x001a},
    {&SHCpu::STSPR               , 0xf0ff, 0x002a},
    {&SHCpu::STSFPSCR            , 0xf0ff, 0x006a}, 
    {&SHCpu::STSFPUL             , 0xf0ff, 0x005a},
    {&SHCpu::STSMMACH            , 0xf0ff, 0x4002},
    {&SHCpu::STSMMACL            , 0xf0ff, 0x4012},
    {&SHCpu::STSMPR              , 0xf0ff, 0x4022},
    {&SHCpu::STSMFPSCR           , 0xf0ff, 0x4062},
    {&SHCpu::STSMFPUL            , 0xf0ff, 0x4052},
    {&SHCpu::TAS                 , 0xf0ff, 0x401b},
    {&SHCpu::FLDI0               , 0xf0ff, 0xf08d},
    {&SHCpu::FLDI1               , 0xf0ff, 0xf09d},
    {&SHCpu::FLOAT               , 0xf0ff, 0xf02d},
    {&SHCpu::FLDS                , 0xf0ff, 0xf01d},
    {&SHCpu::FNEG                , 0xf0ff, 0xf04d},
    {&SHCpu::FSQRT               , 0xf0ff, 0xf06d},
    {&SHCpu::FCNVDS              , 0xf0ff, 0xf0bd},
    {&SHCpu::FCNVSD              , 0xf0ff, 0xf0ad},
    {&SHCpu::FTRC                , 0xf0ff, 0xf03d},
    {&SHCpu::FSTS                , 0xf0ff, 0xf00d},
    {&SHCpu::FSRRA               , 0xf0ff, 0xf07d},
    {&SHCpu::CLRMAC              , 0xffff, 0x0028},
    {&SHCpu::CLRS                , 0xffff, 0x0048},
    {&SHCpu::CLRT                , 0xffff, 0x0008},
    {&SHCpu::DIV0U               , 0xffff, 0x0019},
    {&SHCpu::NOP                 , 0xffff, 0x0009},
    {&SHCpu::RTE                 , 0xffff, 0x002b},
    {&SHCpu::RTS                 , 0xffff, 0x000b},
    {&SHCpu::SETS                , 0xffff, 0x0058},
    {&SHCpu::SETT                , 0xffff, 0x0018},
    {&SHCpu::LDTLB               , 0xffff, 0x0038},
    {&SHCpu::SLEEP               , 0xffff, 0x001b},
    {&SHCpu::FSCA                , 0xf1ff, 0xf0fd}, 
    {&SHCpu::FTRV                , 0xf3ff, 0xf1fd},
    {&SHCpu::FRCHG               , 0xffff, 0xfbfd},
    {&SHCpu::FSCHG               , 0xffff, 0xf3fd},
    {&SHCpu::dispatchSwirlyHook  , 0xffff, 0xfffd}  // last entry
};

#if DYNAREC
static opcode_handler_struct sh_recompile_handler_table[] =
    {
	/*   function                   mask    match */
	{&SHCpu::recBranch           , 0xf0ff, 0x400b},
	{&SHCpu::recBranch           , 0xf000, 0xa000},
	{&SHCpu::recBranch           , 0xff00, 0x8b00},
	{&SHCpu::recBranch           , 0xff00, 0x8f00},
	{&SHCpu::recBranch           , 0xff00, 0x8900},
	{&SHCpu::recBranch           , 0xff00, 0x8d00},
	{&SHCpu::recBranch           , 0xf000, 0xb000},
	{&SHCpu::recBranch           , 0xff00, 0xc300},
	{&SHCpu::recBranch           , 0xf0ff, 0x0023},
	{&SHCpu::recBranch           , 0xf0ff, 0x0003},
	{&SHCpu::recBranch           , 0xf0ff, 0x402b},
	{&SHCpu::recBranch           , 0xffff, 0x002b},
	{&SHCpu::recBranch           , 0xffff, 0x000b},
	{&SHCpu::recNOP              , 0xffff, 0x0009},
	{&SHCpu::recMOV              , 0xf00f, 0x6003},
	{&SHCpu::recHook             , 0xffff, 0xfffd}	// last entry
    };
#endif