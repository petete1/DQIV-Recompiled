// These two SDK entry points used the ARM compiler's literal C++ symbol names.
// Adapt their real implementations to the host C++ ABI. With GCC (the ARM
// compiler's ABI) the first one's name already is the C++ function.
struct G2dSoftSprite;
#ifdef _MSC_VER
extern "C" void _Z16unkfunc_02068ec8i(unsigned int);
void unkfunc_02068ec8(int attributes) { _Z16unkfunc_02068ec8i(static_cast<unsigned int>(attributes)); }
#endif
extern "C" void _Z16unkfunc_02068eecP12UnkG2dSprite(G2dSoftSprite*);
void unkfunc_02068eec(G2dSoftSprite* sprite) { _Z16unkfunc_02068eecP12UnkG2dSprite(sprite); }
