#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdlib.h>
#include "arg.h"
#include "slice.h"
#include "state.h"
#include "cp437.xpm"

#define TITLE "Limonada"
#define COL_BG 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE
#define COL_FG 0, 0, 0, SDL_ALPHA_OPAQUE
#define LETHEIGHT 12
#define LETWIDTH 6
#define BGCOL SDL_SetRenderDrawColor(rend, COL_BG)
#define FGCOL SDL_SetRenderDrawColor(rend, COL_FG)

#define LEFTBARWIDTH 0
#define RIGHTBARWIDTH 0
#define TOBBARHEIGHT ((2*LETHEIGHT)+5)
#define BOTBARHEIGHT (LETHEIGHT+1)
#define DRAWAREAHEIGHT (WINHEIGHT-TOBBARHEIGHT-BOTBARHEIGHT)
#define DRAWAREAWIDTH (WINWIDTH-LEFTBARWIDTH-RIGHTBARWIDTH)

#define MODCTRL 1
#define MODSHIFT 2
#define MODALT 4

int WINWIDTH  = 800;
int WINHEIGHT = 600;

void drawText(SDL_Renderer *rend, char *text, SDL_Texture *font, int x, int y) {
	SDL_Rect srcrect = {0,0,LETWIDTH,LETHEIGHT};
	SDL_Rect destrect = {x,y,LETWIDTH,LETHEIGHT};
	for (;*text!='\0';text++) {
		char c = *text;
		srcrect.x=(c&0x0F)*LETWIDTH;
		srcrect.y=(c>>4)*LETHEIGHT;
		SDL_RenderCopy(rend, font, &srcrect, &destrect);
		destrect.x+=LETWIDTH;
	}
}

typedef SDL_bool (*menuItemCallback)(SDL_Renderer *rend, SDL_Texture *font, limonada *global);

SDL_bool actionQuit(SDL_Renderer* rend, SDL_Texture* font, limonada *global) {
	return SDL_FALSE;
}

typedef struct {
	StrSlice **entries;
	menuItemCallback *callbacks;
	int nentries;
	int width;
} submenu;

typedef struct {
	StrSlice **titles;
	submenu **submenus;
	int ntitles;
	int vis;
} menubar;

menubar *makeMenuBar(char **titles, int ntitles) {
	menubar *ret = malloc(sizeof(menubar));
	ret->submenus = malloc(sizeof(submenu**)*ntitles);
	ret->titles = malloc(sizeof(StrSlice**)*ntitles);
	ret->ntitles = ntitles;
	ret->vis = -1;
	for(int i = 0; i<ntitles; i++) {
		ret->titles[i]=MakeSlice(titles[i]);
	}
	return ret;
}

submenu *makeSubmenu(char **entries, int nentries) {
	submenu *ret = malloc(sizeof(submenu));
	ret->entries = malloc(sizeof(StrSlice**)*nentries);
	ret->callbacks = malloc(sizeof(menuItemCallback)*nentries);
	ret->nentries = nentries;
	ret->width = 0;
	for(int i=0; i<nentries; i++) {
		ret->entries[i]=MakeSlice(entries[i]);
		if(ret->entries[i]->len > ret->width) {
			ret->width = ret->entries[i]->len;
		}
		ret->callbacks[i] = NULL;
	}
	return ret;
}

void drawMenuBar(menubar *m, SDL_Renderer *rend, SDL_Texture *font, int mx, int my) {
	SDL_RenderDrawLine(rend, 0, LETHEIGHT+2, WINWIDTH, LETHEIGHT+2);
	int ix = 0;
	int ox = 0;
	for(int i=0; i<m->ntitles; i++) {
		drawText(rend, m->titles[i]->String, font, ix+2, 1);
		ox = ix;
		ix += (m->titles[i]->len+1)*LETWIDTH;
		if (my < LETHEIGHT && mx >=ox && mx < ix) {
			SDL_Rect r = {ox, 0, ix-ox, LETHEIGHT+2};
			SDL_RenderDrawRect(rend, &r);
			if (m->vis != -1) {
				m->vis = i;
			}
		}
		if (m->vis == i && m->submenus[i] != NULL){
			BGCOL;
			int smw = (m->submenus[i]->width*LETWIDTH)+4;
			SDL_Rect rect = {ox, LETHEIGHT+2, smw,
		        	2+(LETHEIGHT*m->submenus[i]->nentries)};
			SDL_RenderFillRect(rend, &rect);
			FGCOL;
			SDL_RenderDrawRect(rend, &rect);
			for(int j=0; j<m->submenus[i]->nentries; j++) {
				int iy = LETHEIGHT+3+(j*LETHEIGHT);
				drawText(rend, m->submenus[i]->entries[j]->String,
					font, ox+2, iy);
				if (ox <= mx && mx <= ox+smw && iy < my && my <= iy+LETHEIGHT) {
					rect.y=iy;
					rect.h=LETHEIGHT;
					SDL_RenderDrawRect(rend, &rect);
				}
			}
		}
	}
}

void drawTabBar(SDL_Renderer *rend, SDL_Texture *font, limonada *global, menubar *m, int mx, int my) {
	const int botanchor = (2*LETHEIGHT)+4;
	SDL_RenderDrawLine(rend, 0, botanchor, WINWIDTH, botanchor);
	int ix = 0;
	int ox = 0;
	for (int i=0; i<global->buffers->len; i++) {
		StrSlice *bufname = global->buffers->data[i]->name;
		drawText(rend, bufname->String, font, ix+2, botanchor-LETHEIGHT);
		ox = ix;
		ix += (bufname->len+1)*LETWIDTH+2;
		SDL_RenderDrawLine(rend, ix-1, botanchor-LETHEIGHT-1, ix-1, botanchor);
		if (i==global->curbuf) {
			SDL_RenderDrawLine(rend, ox, botanchor-2, ix-1, botanchor-2);
		}
		if (LETHEIGHT+2 < my && my < botanchor && mx >=ox && mx < ix && m->vis==-1) {
			SDL_Rect r = {ox, botanchor-LETHEIGHT-1, ix-ox-1, LETHEIGHT+1};
			SDL_RenderDrawRect(rend, &r);
		}
	}
}

#define BUFSIZE 20
char stat_size[BUFSIZE];
int stat_size_len=0;
char stat_xy[BUFSIZE];
int stat_xy_len=0;
#define ZOOMSIZE 10
char stat_zoom[ZOOMSIZE];
int stat_zoom_len=0;
int px = 0;
int py = 0;
#define UPDATEPXPY px=buf->panx+((mx-LEFTBARWIDTH)/buf->zoom); py=buf->pany+((my-TOBBARHEIGHT)/buf->zoom);stat_xy_len=snprintf(stat_xy, BUFSIZE, "%i,%i", px, py);stat_zoom_len=snprintf(stat_zoom, ZOOMSIZE, "%i%%", buf->zoom*100);

void drawStatBar(SDL_Renderer *rend, SDL_Texture *font, limonada *global) {
	int anchor = WINHEIGHT-BOTBARHEIGHT;
	SDL_RenderDrawLine(rend, 0, anchor, WINWIDTH, anchor);
	if (global->curbuf==-1) {
		drawText(rend, "No buffers open.", font, 0, anchor+1);
		return;
	}

	int ix = 0;
	buffer *buf = global->buffers->data[global->curbuf];
	drawText(rend, buf->name->String, font, ix, anchor+1);
	ix += (buf->name->len+1)*LETWIDTH;
	drawText(rend, "(", font, ix, anchor+1);
	ix+=LETWIDTH;
	drawText(rend, stat_size, font, ix, anchor+1);
	ix += (stat_size_len)*LETWIDTH;
	drawText(rend, ")", font, ix, anchor+1);
	ix+= LETWIDTH*2;
	drawText(rend, "Zoom: ", font, ix, anchor+1);
	ix += 6*LETWIDTH;
	drawText(rend, stat_zoom, font, ix, anchor+1);
	drawText(rend, stat_xy, font, WINWIDTH-(stat_xy_len*LETWIDTH), anchor+1);
}

SDL_Rect drawArea = {LEFTBARWIDTH, TOBBARHEIGHT, 0, 0};
SDL_Rect sprArea = {0, 0, 0, 0};
SDL_Texture *curtext;

void drawBuffer(SDL_Renderer* rend, limonada *global) {
	buffer *buf = global->buffers->data[global->curbuf];
	if (buf->data!=NULL && buf->changedp) {
		buf->changedp = 0;
		curtext = textureFromBuffer(buf, rend);
		stat_size_len = snprintf(stat_size, BUFSIZE, "%i,%i", buf->sizex, buf->sizey);
	}
	int sizex = (buf->sizex-buf->panx)*buf->zoom;
	int sizey = (buf->sizey-buf->pany)*buf->zoom;
	sprArea.x = buf->panx;
	sprArea.y = buf->pany;
	if (sizey == DRAWAREAHEIGHT) {
		drawArea.h = DRAWAREAHEIGHT;
		sprArea.h = sizey;
	} else if (sizey<DRAWAREAHEIGHT) {
		sprArea.h = sizey;
		drawArea.h = sizey;
	} else if (sizey>DRAWAREAHEIGHT) {
		drawArea.h = DRAWAREAHEIGHT;
		sprArea.h = DRAWAREAHEIGHT/buf->zoom;
	}
	if (sizex == DRAWAREAWIDTH) {
		drawArea.w = DRAWAREAWIDTH;
		sprArea.w = sizex;
	} else if (sizex<DRAWAREAWIDTH) {
		sprArea.w = sizex;
		drawArea.w = sizex;
	} else if (sizex>DRAWAREAWIDTH) {
		drawArea.w = DRAWAREAWIDTH;
		sprArea.w = DRAWAREAWIDTH/buf->zoom;
	}
	SDL_RenderCopy(rend, curtext, &sprArea, &drawArea);
}

SDL_Texture* loadFont(SDL_Renderer *rend) {
	SDL_Surface *font;
	font = IMG_ReadXPMFromArray(cp437);
	SDL_Texture *ret = SDL_CreateTextureFromSurface(rend, font);
	SDL_FreeSurface(font);
	return ret;
}

SDL_bool click(SDL_Renderer *rend, SDL_Texture *font, limonada *global, menubar *m, int mx, int my, Uint8 button) {
	if (my <= LETHEIGHT + 2) {
		// Clicked on the menubar
		int ix=0;
		int ox=0;
		for (int i = 0; i<m->ntitles; i++) {
			ox = ix;
			ix += (m->titles[i]->len+1)*LETWIDTH;
			if (mx >=ox && mx < ix) {
				if (i==m->vis) m->vis=-1;
				else m->vis=i;
				return SDL_TRUE;
			}
		}
	} else if (m->vis != -1) {
		// A submenu is visible
		int smw = (m->submenus[m->vis]->width*LETWIDTH)+4;
		int ox = 0;
		for (int i = 0; i<m->vis; i++) ox+=(m->titles[i]->len+1)*LETWIDTH;

		if (ox <= mx && mx <= ox+smw) {
			int iy = LETHEIGHT+3;
			int oy = 0;
			for(int j=0; j<m->submenus[m->vis]->nentries; j++) {
				oy = iy;
				iy += LETHEIGHT;
				if (oy <= my && my <= iy) {
					if (m->submenus[m->vis]->callbacks[j]!=NULL) {
						int sel = m->vis;
						m->vis=-1;
						return (*m->submenus[sel]->callbacks[j])(rend, font, global);
					}
					m->vis=-1;
					return SDL_TRUE;
				}
			}
		}
	} else if (my <= (2*LETHEIGHT)+4) {
		// Clicked on the tab bar
		int ix = 0;
		int ox = 0;
		for (int i=0; i<global->buffers->len; i++) {
			ox = ix;
			ix += (global->buffers->data[i]->name->len+1)*LETWIDTH+2;
			if (mx >=ox && mx < ix) {
				if (button == SDL_BUTTON_MIDDLE) {
					killBufferInList(global->buffers, i);
					if (global->buffers->len == 0) {
						global->curbuf = -1;
					}
				} else if (button == SDL_BUTTON_LEFT) {
					global->curbuf = i;
				}
				if (global->curbuf >= global->buffers->len) {
					global->curbuf = global->buffers->len - 1;
				}
				if (global->curbuf != -1) {
					global->buffers->data[global->curbuf]->changedp=1;
				}
				return SDL_TRUE;
			}
		}
	}
	m->vis=-1;
	return SDL_TRUE;
}

char mods = 0;

void scroll(buffer *buf, SDL_Event event, int mx, int my) {
	if (event.wheel.y > 0) {
		// scroll up
		if (mods&MODCTRL) {
			buf->zoom*=2;
		} else if (mods&MODSHIFT) {
			buf->panx--;
			if (buf->panx<0) {
				buf->panx=0;
			}
		} else {
			buf->pany--;
			if (buf->pany<0) {
				buf->pany=0;
			}
		}
	} else if (event.wheel.y < 0) {
		// scroll down
		if (mods&MODCTRL) {
			buf->zoom/=2;
			if (buf->zoom <= 0) {
				buf->zoom = 1;
			}
		} else if (mods&MODSHIFT) {
			buf->panx++;
			if (buf->panx > buf->sizex) {
				buf->panx=buf->sizex;
			}
		} else {
			buf->pany++;
			if (buf->pany > buf->sizey) {
				buf->pany=buf->sizey;
			}
		}
	}
	UPDATEPXPY;
}

void usage(char *argv0) {
	fprintf(stderr, "usage: %s [files...]\n", argv0);
	exit(1);
}

int main(int argc, char *argv[]) {
	// parse args
	char *argv0 = argv[0];
	ARGBEGIN {
		case 'h':
		default:
			usage(argv0);
	} ARGEND;

	// initialise state
	limonada *global;
	if (argc>0) {
		buflist *buffers = makeBuflistFromArgs(argc, argv);
		global = makeState(buffers);
	} else {
		buflist *buffers = makeBuflist();
		global = makeState(buffers);
	}

	// init sdl
	SDL_Init(SDL_INIT_EVERYTHING);

	// get the window
	SDL_Window *window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, WINWIDTH, WINHEIGHT, SDL_WINDOW_SHOWN
		|SDL_WINDOW_RESIZABLE);
	if (!window) {
		fprintf(stderr, "main: %s\n", SDL_GetError());
		return 1;
	}

	// get the renderer
	SDL_Renderer *rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	// load the font
	SDL_Texture *font = loadFont(rend);

	// Initialise the interface
	int mx, my;
	mx = -1;
	my = -1;
	char *titles[] = {"File","Edit","Help"};
	menubar *m = makeMenuBar(titles, 3);
	char *fileentries[] = {"Open", "Import", "Save", "Export", "Quit"};
	m->submenus[0] = makeSubmenu(fileentries, 5);
	m->submenus[0]->callbacks[4] = *actionQuit;
	char *editentries[] = {"Copy", "Paste"};
	m->submenus[1] = makeSubmenu(editentries, 2);
	char *helpentries[] = {"On-Line Help", "About..."};
	m->submenus[2] = makeSubmenu(helpentries, 2);

	// main loop
	SDL_bool running = SDL_TRUE;
	while (running) {
		// draw the window
		BGCOL;
		SDL_RenderClear(rend);
		FGCOL;
		if (global->curbuf != -1) {
			drawBuffer(rend, global);
		}
		drawStatBar(rend, font, global);
		drawTabBar(rend, font, global, m, mx, my);
		drawMenuBar(m, rend, font, mx, my);
		SDL_RenderPresent(rend);

		// poll for events
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				running = SDL_FALSE;
				break;

			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case SDLK_RCTRL:
				case SDLK_LCTRL:
					mods|=MODCTRL;
					break;
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
					mods|=MODSHIFT;
					break;
				case SDLK_LALT:
				case SDLK_RALT:
					mods|=MODALT;
					break;
				}
				break;

			case SDL_KEYUP:
				switch (event.key.keysym.sym) {
				case SDLK_RCTRL:
				case SDLK_LCTRL:
					mods^=MODCTRL;
					break;
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
					mods^=MODSHIFT;
					break;
				case SDLK_LALT:
				case SDLK_RALT:
					mods^=MODALT;
					break;
				}

			case SDL_MOUSEWHEEL:
				if (global->curbuf != -1) {
					buffer *buf = global->buffers->data[global->curbuf];
					scroll(buf, event, mx, my);
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
				mx = event.button.x;
				my = event.button.y;
				running = click(rend, font, global, m, mx, my, event.button.button);
				break;

			case SDL_MOUSEMOTION:
				mx = event.motion.x;
				my = event.motion.y;
				if (global->curbuf != -1) {
					buffer *buf = global->buffers->data[global->curbuf];
					if (event.motion.state&SDL_BUTTON_MMASK) {
						// If middle button, pan the image
						buf->panx -= event.motion.xrel;
						buf->pany -= event.motion.yrel;
						if (buf->panx > buf->sizex) {
							buf->panx = buf->sizex;
						} else if (buf->panx < 0) {
							buf->panx = 0;
						}
						if (buf->pany > buf->sizey) {
							buf->pany = buf->sizey;
						} else if (buf->pany < 0) {
							buf->pany = 0;
						}
					}
					UPDATEPXPY;
				}
				break;

			case SDL_WINDOWEVENT:
				SDL_GetWindowSize(window, &WINWIDTH, &WINHEIGHT);
				drawArea.w=WINWIDTH;
				drawArea.h=DRAWAREAHEIGHT;
				break;
			}
		}
		SDL_Delay(16);
	}

	// Cleanup and quit back to DOS ;)
	SDL_DestroyWindow(window);
	SDL_Quit();
}

/* Local Variables: */
/* c-basic-offset: 8 */
/* tab-width: 8 */
/* indent-tabs-mode: t */
/* End: */
