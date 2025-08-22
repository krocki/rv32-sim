// RISC-V emulator with SDL2 support - Based on working reference implementation
// Copyright 2022 Charles Lohr, modified for SDL graphics output

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef __APPLE__
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include "default64mbdtc.h"

// Configuration
uint32_t ram_amt = 64*1024*1024;
int fail_on_all_faults = 0;

// SDL Graphics configuration
#define FB_WIDTH  640
#define FB_HEIGHT 480
#define FB_BASE   0x11100000
#define FB_SIZE   (FB_WIDTH * FB_HEIGHT * 4)

// SDL objects
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static uint32_t *framebuffer = NULL;
static int sdl_initialized = 0;
static int should_quit = 0;

// Function declarations
static int64_t SimpleReadNumberInt( const char * number, int64_t defaultNumber );
static uint64_t GetTimeMicroseconds();
static void ResetKeyboardInput();
static void CaptureKeyboardInput();
static uint32_t HandleException( uint32_t ir, uint32_t retval );
static uint32_t HandleControlStore( uint32_t addy, uint32_t val );
static uint32_t HandleControlLoad( uint32_t addy );
static void HandleOtherCSRWrite( uint8_t * image, uint16_t csrno, uint32_t value );
static int32_t HandleOtherCSRRead( uint8_t * image, uint16_t csrno );
static void MiniSleep();
static int IsKBHit();
static int ReadKBByte();

// Emulator configuration
#define MINIRV32WARN( x... ) printf( x );
#define MINIRV32_DECORATE  static
#define MINI_RV32_RAM_SIZE ram_amt
#define MINIRV32_IMPLEMENTATION
#define MINIRV32_POSTEXEC( pc, ir, retval ) { if( retval > 0 ) { if( fail_on_all_faults ) { printf( "FAULT\n" ); return 3; } else retval = HandleException( ir, retval ); } }
#define MINIRV32_HANDLE_MEM_STORE_CONTROL( addy, val ) if( HandleControlStore( addy, val ) ) return val;
#define MINIRV32_HANDLE_MEM_LOAD_CONTROL( addy, rval ) rval = HandleControlLoad( addy );
#define MINIRV32_OTHERCSR_WRITE( csrno, value ) HandleOtherCSRWrite( image, csrno, value );
#define MINIRV32_OTHERCSR_READ( csrno, value ) value = HandleOtherCSRRead( image, csrno );

#include "mini-rv32ima.h"

uint8_t * ram_image = 0;
struct MiniRV32IMAState * core;
const char * kernel_command_line = 0;
static void DumpState( struct MiniRV32IMAState * core, uint8_t * ram_image );

// SDL Functions
static int InitSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        return -1;
    }
    
    window = SDL_CreateWindow("RISC-V SDL DOOM Emulator",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              FB_WIDTH, FB_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        return -1;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        return -1;
    }
    
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING,
                               FB_WIDTH, FB_HEIGHT);
    if (!texture) {
        fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
        return -1;
    }
    
    framebuffer = (uint32_t*)calloc(FB_WIDTH * FB_HEIGHT, sizeof(uint32_t));
    if (!framebuffer) {
        fprintf(stderr, "Failed to allocate framebuffer\n");
        return -1;
    }
    
    sdl_initialized = 1;
    printf("SDL2 Display initialized (%dx%d)\n", FB_WIDTH, FB_HEIGHT);
    return 0;
}

static void UpdateSDL() {
    if (!sdl_initialized) return;
    
    SDL_UpdateTexture(texture, NULL, framebuffer, FB_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

static void CleanupSDL() {
    if (framebuffer) free(framebuffer);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

static void HandleSDLEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || 
            (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
            should_quit = 1;
        }
    }
}

int main( int argc, char ** argv )
{
    int i;
    long long instct = -1;
    int show_help = 0;
    int time_divisor = 1;
    int fixed_update = 0;
    int enable_printf = 1;
    int disable_sdl = 0;
    int single_step = 0;
    int dtb_ptr = 0;
    const char * image_file_name = 0;
    const char * dtb_file_name = 0;
    
    for( i = 1; i < argc; i++ )
    {
        const char * param = argv[i];
        int param_continue = 0;
        if( param[0] == '-' )
        {
            switch( param[1] )
            {
                case 'm': ram_amt = SimpleReadNumberInt( argv[++i], ram_amt ); break;
                case 'c': instct = SimpleReadNumberInt( argv[++i], -1 ); break;
                case 'k': kernel_command_line = argv[++i]; break;
                case 'f': image_file_name = argv[++i]; break;
                case 'b': dtb_file_name = argv[++i]; break;
                case 'l': param_continue = 1; fixed_update = 1; break;
                case 'p': param_continue = 1; enable_printf = 0; break;
                case 's': param_continue = 1; single_step = 1; break;
                case 'd': param_continue = 1; fail_on_all_faults = 1; break;
                case 't': time_divisor = SimpleReadNumberInt( argv[++i], 1 ); break;
                case 'n': disable_sdl = 1; break;  // Option to disable SDL
                default:
                    if( param_continue )
                        continue;
                    else
                        show_help = 1;
                    break;
            }
        }
        else
        {
            show_help = 1;
            break;
        }
    }

    if( show_help || image_file_name == 0 || time_divisor <= 0 )
    {
        fprintf( stderr, "RISC-V SDL Emulator\n" );
        fprintf( stderr, "Usage: %s -f [image] [options]\n", argv[0] );
        fprintf( stderr, "  -m [ram amount]         (default: %d)\n", ram_amt );
        fprintf( stderr, "  -f [running image]      (required)\n" );
        fprintf( stderr, "  -k [kernel command line]\n" );
        fprintf( stderr, "  -b [dtb file, or 'disable']\n" );
        fprintf( stderr, "  -c [instruction count]  (default: -1 = run forever)\n" );
        fprintf( stderr, "  -s                      single step with full state\n" );
        fprintf( stderr, "  -t [time divisor]       (default: 1)\n" );
        fprintf( stderr, "  -l                      lock time base to instruction count\n" );
        fprintf( stderr, "  -p                      disable printf\n" );
        fprintf( stderr, "  -d                      fail on all faults\n" );
        fprintf( stderr, "  -n                      disable SDL (console only)\n" );
        return 1;
    }

    // Initialize SDL if not disabled
    if (!disable_sdl) {
        if (InitSDL() < 0) {
            fprintf(stderr, "Warning: SDL initialization failed, continuing without graphics\n");
            disable_sdl = 1;
        }
    }

    ram_image = malloc( ram_amt );
    if( !ram_image )
    {
        fprintf( stderr, "Error: could not allocate system image.\n" );
        return -4;
    }
    memset( ram_image, 0, ram_amt );

    // Load the image
    FILE * f = fopen( image_file_name, "rb" );
    if( !f || ferror( f ) )
    {
        fprintf( stderr, "Error: \"%s\" not found\n", image_file_name );
        return -5;
    }
    fseek( f, 0, SEEK_END );
    long flen = ftell( f );
    fseek( f, 0, SEEK_SET );
    if( flen > ram_amt )
    {
        fprintf( stderr, "Error: Could not fit RAM image (%ld bytes) into %d\n", flen, ram_amt );
        return -6;
    }
    
    if( fread( ram_image, flen, 1, f ) != 1 )
    {
        fprintf( stderr, "Error: Could not load image.\n" );
        return -7;
    }
    fclose( f );
    
    printf("Image loaded: %s (%ld bytes)\n", image_file_name, flen);

    // Handle DTB
    if( dtb_file_name )
    {
        if( strcmp( dtb_file_name, "disable" ) == 0 )
        {
            dtb_ptr = 0;
        }
        else
        {
            FILE * f = fopen( dtb_file_name, "rb" );
            if( !f || ferror( f ) )
            {
                fprintf( stderr, "Error: \"%s\" not found\n", dtb_file_name );
                return -5;
            }
            fseek( f, 0, SEEK_END );
            long flen = ftell( f );
            fseek( f, 0, SEEK_SET );
            dtb_ptr = ram_amt - flen - 0x1000;
            if( fread( ram_image + dtb_ptr, flen, 1, f ) != 1 )
            {
                fprintf( stderr, "Error: Could not open dtb \"%s\"\n", dtb_file_name );
                return -9;
            }
            fclose( f );
        }
    }
    else
    {
        // Load default DTB
        dtb_ptr = ram_amt - sizeof(default64mbdtb) - 0x1000;
        memcpy( ram_image + dtb_ptr, default64mbdtb, sizeof( default64mbdtb ) );
    }

    // Initialize core
    core = (struct MiniRV32IMAState *)calloc( sizeof( struct MiniRV32IMAState ), 1 );
    core->pc = MINIRV32_RAM_IMAGE_OFFSET;
    core->regs[10] = 0x00;
    core->regs[11] = dtb_ptr ? (dtb_ptr + MINIRV32_RAM_IMAGE_OFFSET) : 0;
    core->extraflags |= 3;

    // Setup terminal
    CaptureKeyboardInput();

    uint64_t rt;
    uint64_t lastTime = GetTimeMicroseconds() / time_divisor;
    int instrs_per_flip = 1024;
    int update_counter = 0;

    printf("Starting emulation... Press ESC to quit\n");
    
    for( rt = 0; rt < instct && !should_quit; rt += instrs_per_flip )
    {
        // Handle SDL events
        if (!disable_sdl) {
            HandleSDLEvents();
        }
        
        uint64_t * this_ccount = ((uint64_t*)&core->cyclel);
        uint32_t elapsedUs = 0;
        if( fixed_update )
        {
            *this_ccount += instrs_per_flip;
        }
        else
        {
            uint64_t currentTime = GetTimeMicroseconds() / time_divisor;
            elapsedUs = currentTime - lastTime;
            lastTime = currentTime;
            *this_ccount += elapsedUs;
        }

        int ret = MiniRV32IMAStep( core, ram_image, 0, elapsedUs, instrs_per_flip );
        if( ret )
        {
            if( ret == 0x1234 )
            {
                should_quit = 1;
            }
            else if( ret == 0x7777 )
            {
                printf( "Restart\n" );
                core->cyclel = 0;
                core->cycleh = 0;
                core->timerl = 0;
                core->timerh = 0;
                core->pc = MINIRV32_RAM_IMAGE_OFFSET;
                core->regs[10] = 0x00;
                core->regs[11] = dtb_ptr ? (dtb_ptr + MINIRV32_RAM_IMAGE_OFFSET) : 0;
                core->extraflags |= 3;
            }
            else
            {
                printf( "Fault: %d at PC=%08x\n", ret, core->pc );
                should_quit = 1;
            }
        }

        // Update SDL display periodically
        if (!disable_sdl && ++update_counter > 100) {
            UpdateSDL();
            update_counter = 0;
        }

        if( single_step )
        {
            DumpState( core, ram_image );
        }
    }

    // Cleanup
    printf("\nEmulation ended. Total instructions: %lld\n", rt);
    ResetKeyboardInput();
    
    if (!disable_sdl) {
        CleanupSDL();
    }
    
    free( ram_image );
    free( core );
    return 0;
}

// ============= Support Functions =============

static uint32_t HandleException( uint32_t ir, uint32_t code )
{
    // Just move on
    return 0;
}

static uint32_t HandleControlStore( uint32_t addy, uint32_t val )
{
    // Framebuffer writes
    if( sdl_initialized && addy >= FB_BASE && addy < FB_BASE + FB_SIZE )
    {
        uint32_t offset = (addy - FB_BASE) / 4;
        if( offset < FB_WIDTH * FB_HEIGHT )
        {
            framebuffer[offset] = val;
        }
        return 0;
    }
    
    // UART output
    if( addy == 0x10000000 )
    {
        putchar( val );
        fflush( stdout );
    }
    else if( addy == 0x11004004 )
    {
        // Timer match
        core->timermatchh = val;
    }
    else if( addy == 0x11004000 )
    {
        // Timer match low
        core->timermatchl = val;
    }
    else if( addy == 0x11100000 )
    {
        // Power off
        if( val == 0x5555 )
        {
            return 0x1234;
        }
    }
    return 0;
}

static uint32_t HandleControlLoad( uint32_t addy )
{
    // Framebuffer reads
    if( sdl_initialized && addy >= FB_BASE && addy < FB_BASE + FB_SIZE )
    {
        uint32_t offset = (addy - FB_BASE) / 4;
        if( offset < FB_WIDTH * FB_HEIGHT )
        {
            return framebuffer[offset];
        }
        return 0;
    }
    
    // UART/Keyboard input
    if( addy == 0x10000000 )
    {
        if( IsKBHit() )
        {
            return 0x100 | ReadKBByte();
        }
        return 0;
    }
    else if( addy == 0x10000005 )
    {
        return 0x60;
    }
    else if( addy >= 0x11000000 && addy < 0x11001000 )
    {
        // Timer
        uint64_t ts = GetTimeMicroseconds();
        if( addy == 0x1100bff8 )
            return ts & 0xffffffff;
        else if( addy == 0x1100bffc )
            return (ts >> 32) & 0xffffffff;
    }
    return 0;
}

static void HandleOtherCSRWrite( uint8_t * image, uint16_t csrno, uint32_t value )
{
    if( csrno == 0x136 )
    {
        printf( "%d", value );
        fflush( stdout );
    }
}

static int32_t HandleOtherCSRRead( uint8_t * image, uint16_t csrno )
{
    if( csrno == 0x140 )
    {
        return core->cyclel;
    }
    return 0;
}

static void MiniSleep()
{
    usleep(1);
}

// Platform-specific keyboard handling
#ifdef _WIN32
#include <windows.h>
#include <conio.h>

static int IsKBHit() { return _kbhit(); }
static int ReadKBByte() { return _getch(); }
static void CaptureKeyboardInput() {}
static void ResetKeyboardInput() {}

#else

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

static struct termios oldt, newt;

static void ResetKeyboardInput()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

static void CaptureKeyboardInput()
{
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_iflag &= ~(ICRNL);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

static int IsKBHit()
{
    int byteswaiting;
    ioctl(0, FIONREAD, &byteswaiting);
    return byteswaiting > 0;
}

static int ReadKBByte()
{
    unsigned char c;
    if( read(0, &c, 1) != 1 ) return 0;
    return c;
}
#endif

static uint64_t GetTimeMicroseconds()
{
    struct timeval tv;
    gettimeofday( &tv, 0 );
    return tv.tv_usec + ((uint64_t)(tv.tv_sec)) * 1000000LL;
}

static int64_t SimpleReadNumberInt( const char * number, int64_t defaultNumber )
{
    if( !number || !number[0] ) return defaultNumber;
    int radix = 10;
    if( number[0] == '0' )
    {
        char nc = number[1];
        number+=2;
        if( nc == 0 ) return 0;
        else if( nc == 'x' ) radix = 16;
        else if( nc == 'b' ) radix = 2;
        else { number--; radix = 8; }
    }
    char * endptr;
    uint64_t ret = strtoll( number, &endptr, radix );
    if( endptr == number )
    {
        return defaultNumber;
    }
    else
    {
        return ret;
    }
}

static void DumpState( struct MiniRV32IMAState * core, uint8_t * ram_image )
{
    uint32_t pc = core->pc;
    uint32_t pc_offset = pc - MINIRV32_RAM_IMAGE_OFFSET;
    uint32_t ir = 0;

    printf( "PC: %08x ", pc );
    if( pc_offset >= 0 && pc_offset < ram_amt - 3 )
    {
        ir = *(uint32_t*)(ram_image + pc_offset);
        printf( "[0x%08x] ", ir );
    }
    else
        printf( "[xxxxxxxxxx] " );
    
    // Dump registers
    int reg;
    for( reg = 0; reg < 32; reg++ )
    {
        printf( "x%d=%08x ", reg, core->regs[reg] );
        if( (reg & 7) == 7 ) printf( "\n" );
    }
    printf( "\n" );
}
