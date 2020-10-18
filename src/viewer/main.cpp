#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <time.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avstring.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <SDL2/SDL.h>

int main()
{

    int res = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
    if (res != 0)
    {
        printf("SDL_Init failed - %s\n.", SDL_GetError());
        return -1;
    }

    auto ctx = sws_getContext(1920,
                   1080,
                   AV_PIX_FMT_YUV420P,
                   1920,
                   1080,
                   AV_PIX_FMT_YUV420P,
                SWS_BILINEAR,
                NULL,
                NULL,
                NULL
            );

    auto screen = SDL_CreateWindow(
            "Media Timeline",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            800,
            600,
            SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!screen)
    {
        printf("SDL_CreateWindow failed - exiting.\n");
        return -1;
    }

    auto renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);

    auto texture =  SDL_CreateTexture(
                    renderer,
                    SDL_PIXELFORMAT_YV12,
                    SDL_TEXTUREACCESS_STREAMING,
                    1920,
                    1080
            );

        SDL_Rect rect;
            rect.x = 0;
            rect.y = 100;
            rect.w = 1920;
            rect.h = 1080;

            SDL_UpdateYUVTexture(
                    texture,
                    &rect,
                    videoPicture->frame->data[0],
                    videoPicture->frame->linesize[0],
                    videoPicture->frame->data[1],
                    videoPicture->frame->linesize[1],
                    videoPicture->frame->data[2],
                    videoPicture->frame->linesize[2]
            );


    SDL_Event event;
    bool exit_signal = false;
    while(!exit_signal)
    {
        res = SDL_WaitEvent(&event);
        if (res == 0)
        {
            printf("SDL_WaitEvent failed: %s.\n", SDL_GetError());
        }

        // switch on the retrieved event type
        switch(event.type)
        {
            case SDL_KEYDOWN:
            {
                switch(event.key.keysym.sym)
                {
                    case SDLK_LEFT:
                    {
                        printf ("Left\n");
                        break;
                    }
                    case SDLK_RIGHT:
                    {
                        printf ("Right\n");
                        break;
                    }
                    case SDLK_UP:
                    {
                        printf ("Up\n");
                        break;
                    }
                    case SDLK_DOWN:
                    {
                        printf ("Down\n");
                        break;
                    }
                    case SDLK_ESCAPE:
                    {
                        exit_signal = true;
                        break;
                    }
                }
                break;
            }

            case SDL_QUIT:
            {
                exit_signal = true;
                break;
            }
        }
    }

    SDL_Quit();


    return 0;
}