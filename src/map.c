/*
 * See Licensing and Copyright notice in naev.h
 */


#include "map.h"

#include "naev.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "log.h"
#include "toolkit.h"
#include "space.h"
#include "opengl.h"
#include "mission.h"
#include "colour.h"
#include "player.h"
#include "faction.h"
#include "dialogue.h"
#include "gui.h"
#include "map_find.h"
#include "array.h"
#include "mapData.h"
#include "nstring.h"
#include "nmath.h"
#include "nmath.h"
#include "nxml.h"
#include "ndata.h"
#include "map_system.h"

#define BUTTON_WIDTH    100 /**< Map button width. */
#define BUTTON_HEIGHT   30 /**< Map button height. */


#define MAP_LOOP_PROT   1000 /**< Number of iterations max in pathfinding before
                                 aborting. */

/* map decorator stack */
static MapDecorator* decorator_stack = NULL; /**< Contains all the map decorators. */
static int decorator_nstack       = 0; /**< Number of map decorators in the stack. */

static double map_zoom        = 1.; /**< Zoom of the map. */
static double map_xpos        = 0.; /**< Map X position. */
static double map_ypos        = 0.; /**< Map Y position. */
static int map_drag           = 0; /**< Is the user dragging the map? */
static int map_selected       = -1; /**< What system is selected on the map. */
static StarSystem **map_path  = NULL; /**< The path to current selected system. */
int map_npath                 = 0; /**< Number of systems in map_path. */
glTexture *gl_faction_disk    = NULL; /**< Texture of the disk representing factions. */
static int cur_commod         = -1; /**< Current commodity selected. */
static int cur_commod_mode    = 0; /**< 0 for difference, 1 for cost. */
static int commod_counter = 0; /**< used to fade back in the faction smudges */
static Commodity **commod_known = NULL; /**< index of known commodities */
static int nmap_modes = 0; /**< number of map modes (depends on number of commodities) */
static char** map_modes = NULL; /**< Holds the names of the different map modes. */
static int listMapModeVisible = 0; /**< Whether the map mode list widget is visible. */
static double commod_av_gal_price = 0; /**< Average price across the galaxy. */
/* VBO. */
static gl_vbo *map_vbo = NULL; /**< Map VBO. */
static gl_vbo *marker_vbo = NULL;

/*
 * extern
 */
/* space.c */
extern StarSystem *systems_stack;
extern int systems_nstack;
extern int faction_nstack;

/*land.c*/
extern int landed;
extern Planet* land_planet;

/*
 * prototypes
 */
/* Update. */
static void map_update( unsigned int wid );
/* Render. */
static void map_render( double bx, double by, double w, double h, void *data );
static void map_renderPath( double x, double y, double a );
static void map_renderMarkers( double x, double y, double r, double a );
static void map_renderCommod( double bx, double by, double x, double y,
                              double w, double h, double r, int editor);
static void map_drawMarker( double x, double y, double r, double a,
      int num, int cur, int type );
/* Mouse. */
static int map_mouse( unsigned int wid, SDL_Event* event, double mx, double my,
      double w, double h, void *data );
/* Misc. */
static glTexture *gl_genFactionDisk( int radius );
static int map_keyHandler( unsigned int wid, SDL_Keycode key, SDL_Keymod mod );
static void map_buttonZoom( unsigned int wid, char* str );
static void map_buttonCommodity( unsigned int wid, char* str );
static void map_selectCur (void);
static void map_genModeList(void);
static void map_update_commod_av_price();
static void map_window_close( unsigned int wid, char *str );


/**
 * @brief Initializes the map subsystem.
 *
 *    @return 0 on success.
 */
int map_init (void)
{
   const double beta = M_PI / 9;
   GLfloat vertex[6];

   /* Create the VBO. */
   map_vbo = gl_vboCreateStream( sizeof(GLfloat) * 3*(2+4), NULL );

   vertex[0] = 1;
   vertex[1] = 0;
   vertex[2] = 1 + 3 * cos(beta);
   vertex[3] = 3 * sin(beta);
   vertex[4] = 1 + 3 * cos(beta);
   vertex[5] = -3 * sin(beta);
   marker_vbo = gl_vboCreateStatic( sizeof(GLfloat) * 6, vertex );

   gl_faction_disk = gl_genFactionDisk( 150. );
   return 0;
}


/**
 * @brief Destroys the map subsystem.
 */
void map_exit (void)
{
   int i;

   /* Destroy the VBO. */
   if (map_vbo != NULL) {
      gl_vboDestroy(map_vbo);
      map_vbo = NULL;
   }

   if (gl_faction_disk != NULL)
      gl_freeTexture( gl_faction_disk );

   if (decorator_stack != NULL) {
      for (i=0; i<decorator_nstack; i++)
         gl_freeTexture( decorator_stack[i].image );
      free( decorator_stack );
      decorator_stack = NULL;
      decorator_nstack = 0;
   }
}


/**
 * @brief Handles key input to the map window.
 */
static int map_keyHandler( unsigned int wid, SDL_Keycode key, SDL_Keymod mod )
{
   (void) mod;

   if ((key == SDLK_SLASH) || (key == SDLK_f)) {
      map_inputFind( wid, NULL );
      return 1;
   }

   return 0;
}


/**
 * @brief Opens the map window.
 */
void map_open (void)
{
   unsigned int wid;
   StarSystem *cur;
   int w, h, x, y, rw;
   /* Not displaying commodities */
   cur_commod = -1;
   listMapModeVisible = 0;

   /* Not under manual control. */
   if (pilot_isFlag( player.p, PILOT_MANUAL_CONTROL ))
      return;

   /* Destroy window if exists. */
   wid = window_get(MAP_WDWNAME);
   if (wid > 0) {
      window_destroy( wid );
      return;
   }

   /* set position to focus on current system */
   map_xpos = cur_system->pos.x;
   map_ypos = cur_system->pos.y;

   /* mark systems as needed */
   mission_sysMark();

   /* Attempt to select current map if none is selected */
   if (map_selected == -1)
      map_selectCur();

   /* get the selected system. */
   cur = system_getIndex( map_selected );

   /* Set up window size. */
   w = MAX(600, SCREEN_W - 100);
   h = MAX(540, SCREEN_H - 100);

   /* create the window. */
   wid = window_create( MAP_WDWNAME, -1, -1, w, h );
   window_setCancel( wid, map_window_close );
   window_handleKeys( wid, map_keyHandler );

   /*
    * SIDE TEXT
    *
    * $System
    *
    * Faction:
    *   $Faction (or Multiple)
    *
    * Status:
    *   $Status
    *
    * Planets:
    *   $Planet1, $Planet2, ...
    *
    * Services:
    *   $Services
    *
    * ...
    * [Autonav]
    * [ Find ]
    * [ Close ]
    */

   x  = -70; /* Right column X offset. */
   y  = -20;
   rw = ABS(x) + 60; /* Right column indented width maximum. */

   /* System Name */
   window_addText( wid, -90 + 80, y, 160, 20, 1, "txtSysname",
         &gl_defFont, NULL, cur->name );
   y -= 10;

   /* Faction image */
   window_addImage( wid, -90 + 32, y - 32, 0, 0, "imgFaction", NULL, 0 );
   y -= 64 + 10;

   /* Faction */
   window_addText( wid, x, y, 90, 20, 0, "txtSFaction",
         &gl_smallFont, NULL, _("Faction:") );
   window_addText( wid, x + 50, y-gl_smallFont.h-5, rw, 100, 0, "txtFaction",
         &gl_smallFont, NULL, NULL );
   y -= 2 * gl_smallFont.h + 5 + 15;

   /* Standing */
   window_addText( wid, x, y, 90, 20, 0, "txtSStanding",
         &gl_smallFont, NULL, _("Standing:") );
   window_addText( wid, x + 50, y-gl_smallFont.h-5, rw, 100, 0, "txtStanding",
         &gl_smallFont, NULL, NULL );
   y -= 2 * gl_smallFont.h + 5 + 15;

   /* Presence. */
   window_addText( wid, x, y, 90, 20, 0, "txtSPresence",
         &gl_smallFont, NULL, _("Presence:") );
   window_addText( wid, x + 50, y-gl_smallFont.h-5, rw, 100, 0, "txtPresence",
         &gl_smallFont, NULL, NULL );
   y -= 2 * gl_smallFont.h + 5 + 15;

   /* Planets */
   window_addText( wid, x, y, 90, 20, 0, "txtSPlanets",
         &gl_smallFont, NULL, _("Planets:") );
   window_addText( wid, x + 50, y-gl_smallFont.h-5, rw, 150, 0, "txtPlanets",
         &gl_smallFont, NULL, NULL );
   y -= 2 * gl_smallFont.h + 5 + 15;

   /* Services */
   window_addText( wid, x, y, 90, 20, 0, "txtSServices",
         &gl_smallFont, NULL, _("Services:") );
   window_addText( wid, x + 50, y-gl_smallFont.h-5, rw, 100, 0, "txtServices",
         &gl_smallFont, NULL, NULL );

   /* Close button */
   window_addButton( wid, -20, 20, BUTTON_WIDTH, BUTTON_HEIGHT,
            "btnClose", _("Close"), map_window_close );
   /* Commodity button */
   window_addButton( wid, -20 - (BUTTON_WIDTH+20), 20, BUTTON_WIDTH, BUTTON_HEIGHT, "btnCommod", _("Mode"), map_buttonCommodity );
   /* Find button */
   window_addButton( wid, -20 - 2*(BUTTON_WIDTH+20), 20, BUTTON_WIDTH, BUTTON_HEIGHT,
            "btnFind", _("Find"), map_inputFind );
   /* Autonav button */
   window_addButton( wid, -20 - 3*(BUTTON_WIDTH+20), 20, BUTTON_WIDTH, BUTTON_HEIGHT,
            "btnAutonav", _("Autonav"), player_autonavStartWindow );

   /*
    * Bottom stuff
    *
    * [+] [-]  Nebula, Interference
    */
   /* Zoom buttons */
   window_addButton( wid, -60, 40 + BUTTON_HEIGHT, 30, BUTTON_HEIGHT, "btnZoomIn", "+", map_buttonZoom );
   window_addButton( wid, -20, 40 + BUTTON_HEIGHT, 30, BUTTON_HEIGHT, "btnZoomOut", "-", map_buttonZoom );
   /* Situation text */
   window_addText( wid, 20, 10, w - 120 - 4*BUTTON_WIDTH, 30, 0,
                   "txtSystemStatus", &gl_smallFont, NULL, NULL );

   map_genModeList();

   /*
    * The map itself.
    */
   map_show( wid, 20, -40, w-200, h-100, 1. ); /* Reset zoom. */

   map_update( wid );

   /*
    * Disable Autonav button if player lacks fuel or if target is not a valid hyperspace target.
    */
   if ((player.p->fuel < player.p->fuel_consumption) || pilot_isFlag( player.p, PILOT_NOJUMP)
         || map_selected == cur_system - systems_stack || map_npath == 0)
      window_disableButton( wid, "btnAutonav" );
}

/*
 * Prepares economy info for rendering.  Called when cur_commod changes.
 */

static void map_update_commod_av_price()
{
   Commodity *c;
   int i,j,k;
   StarSystem *sys;
   Planet *p;
   if (cur_commod == -1 || map_selected == -1) {
      commod_av_gal_price = 0;
      return;
   }
   c=commod_known[cur_commod];
   if ( cur_commod_mode !=0 ) {
      double totPrice = 0;
      int totPriceCnt = 0;
      for (i=0; i<systems_nstack; i++) {
         sys = system_getIndex( i );

         /* if system is not known, reachable, or marked. and we are not in the editor */
         if ((!sys_isKnown(sys) && !sys_isFlag(sys, SYSTEM_MARKED | SYSTEM_CMARKED)
              && !space_sysReachable(sys)))
            continue;
         if ((sys_isKnown(sys)) && (system_hasPlanet(sys))) {
            double sumPrice=0;
            int sumCnt=0;
            double thisPrice;
            for ( j=0 ; j<sys->nplanets; j++) {
               p=sys->planets[j];
               for ( k=0; k<p->ncommodities; k++) {
                  if ( p->commodities[k] == c ) {
                     if ( p->commodityPrice[k].cnt > 0 ) {/*commodity is known about*/
                        thisPrice = p->commodityPrice[k].sum / p->commodityPrice[k].cnt;
                        sumPrice+=thisPrice;
                        sumCnt+=1;
                        break;
                     }
                  }
               }
            }
            if ( sumCnt>0 ) {
               totPrice += sumPrice / sumCnt;
               totPriceCnt++;
            }
         }
      }
      if ( totPriceCnt > 0 )
         totPrice /= totPriceCnt;
      commod_av_gal_price = totPrice;

   } else {
      commod_av_gal_price = 0;
   }
}

/**
 * @brief Updates the map window.
 *
 *    @param wid Window id.
 */
static void map_update( unsigned int wid )
{
   int i;
   StarSystem *sys;
   int f, h, x, y;
   unsigned int services;
   int l;
   int hasPresence, hasPlanets;
   char t;
   const char *sym, *adj;
   char buf[PATH_MAX];
   int p;
   glTexture *logo;
   double w;
   double unknownPresence;
   Commodity *c;

   /* Needs map to update. */
   if (!map_isOpen())
      return;

   /* Get selected system. */
   sys = system_getIndex( map_selected );

   /* Not known and no markers. */
   if (!(sys_isFlag(sys, SYSTEM_MARKED | SYSTEM_CMARKED)) &&
         !sys_isKnown(sys) && !space_sysReachable(sys)) {
      map_selectCur();
      sys = system_getIndex( map_selected );
   }
   /* Average commodity price */
   map_update_commod_av_price();

   /* Economy button */
   if ( cur_commod >= 0 ) {
      c = commod_known[cur_commod];
      if ( cur_commod_mode == 0 ) {
         if (sys!=NULL) {
            snprintf(buf,PATH_MAX,"%s prices trading from %s shown: Positive/blue values mean a profit\nwhile negative/orange values mean a loss when sold at the corresponding system.",c->name,sys->name);
            window_modifyText( wid, "txtSystemStatus", buf );
         }
      } else {
         snprintf(buf,PATH_MAX,"Known %s prices shown. Galaxy-wide average: %.2f",c->name,commod_av_gal_price);
         window_modifyText( wid, "txtSystemStatus", buf );

      }
   } else {
      window_modifyText( wid, "txtSystemStatus", NULL );
   }

   /*
    * Right Text
    */

   x = -70; /* Side bar X offset. */
   w = ABS(x) + 60; /* Width of the side bar. */
   y = -20 - 20 - 64 - gl_defFont.h; /* Initialized to position for txtSFaction. */

   if (!sys_isKnown(sys)) { /* System isn't known, erase all */
      /*
       * Right Text
       */
      if (sys_isFlag(sys, SYSTEM_MARKED | SYSTEM_CMARKED))
         window_modifyText( wid, "txtSysname", sys->name );
      else
         window_modifyText( wid, "txtSysname", _("Unknown") );;

      /* Faction */
      window_modifyImage( wid, "imgFaction", NULL, 0, 0 );
      window_moveWidget( wid, "txtSFaction", x, y);
      window_moveWidget( wid, "txtFaction", x + 50, y - gl_smallFont.h - 5 );
      window_modifyText( wid, "txtFaction", _("Unknown") );
      y -= 2 * gl_smallFont.h + 5 + 15;

      /* Standing */
      window_moveWidget( wid, "txtSStanding", x, y );
      window_moveWidget( wid, "txtStanding", x + 50, y - gl_smallFont.h - 5 );
      window_modifyText( wid, "txtStanding", _("Unknown") );
      y -= 2 * gl_smallFont.h + 5 + 15;

      /* Presence. */
      window_moveWidget( wid, "txtSPresence", x, y );
      window_moveWidget( wid, "txtPresence",  x + 50, y - gl_smallFont.h - 5 );
      window_modifyText( wid, "txtPresence", _("Unknown") );
      y -= 2 * gl_smallFont.h + 5 + 15;

      /* Planets */
      window_moveWidget( wid, "txtSPlanets", x, y );
      window_moveWidget( wid, "txtPlanets", x + 50, y - gl_smallFont.h - 5 );
      window_modifyText( wid, "txtPlanets", _("Unknown") );
      y -= 2 * gl_smallFont.h + 5 + 15;

      /* Services */
      window_moveWidget( wid, "txtSServices", x, y );
      window_moveWidget( wid, "txtServices", x + 50, y -gl_smallFont.h - 5 );
      window_modifyText( wid, "txtServices", _("Unknown") );

      /*
       * Bottom Text
       */
      window_modifyText( wid, "txtSystemStatus", NULL );
      return;
   }

   /* System is known */
   window_modifyText( wid, "txtSysname", sys->name );

   f         = -1;
   for (i=0; i<sys->nplanets; i++) {
      if (sys->planets[i]->real != ASSET_REAL)
         continue;
      if (!planet_isKnown(sys->planets[i]))
         continue;
      if ( (sys->planets[i]->faction > 0)
            && (!faction_isKnown(sys->planets[i]->faction)) )
         continue;

      if ((f == -1) && (sys->planets[i]->faction > 0)) {
         f = sys->planets[i]->faction;
      }
      else if (f != sys->planets[i]->faction /** @todo more verbosity */
               && (sys->planets[i]->faction > 0)) {
         nsnprintf( buf, PATH_MAX, _("Multiple") );
         break;
      }
   }
   if (f == -1) {
      window_modifyImage( wid, "imgFaction", NULL, 0, 0 );
      window_modifyText( wid, "txtFaction", _("N/A") );
      window_modifyText( wid, "txtStanding", _("N/A") );
      h = gl_smallFont.h;
   }
   else {
      if (i==sys->nplanets) /* saw them all and all the same */
         nsnprintf( buf, PATH_MAX, "%s", faction_longname(f) );

      /* Modify the image. */
      logo = faction_logoSmall(f);
      window_modifyImage( wid, "imgFaction", logo, 0, 0 );
      if (logo != NULL)
         window_moveWidget( wid, "imgFaction",
               -90 + logo->w/2, -20 - 32 - 10 - gl_defFont.h + logo->h/2);

      /* Modify the text */
      window_modifyText( wid, "txtFaction", buf );
      window_modifyText( wid, "txtStanding",
            faction_getStandingText( f ) );

      h = gl_printHeightRaw( &gl_smallFont, w, buf );
   }

   /* Faction */
   window_moveWidget( wid, "txtSFaction", x, y);
   window_moveWidget( wid, "txtFaction", x + 50, y - gl_smallFont.h - 5 );
   y -= gl_smallFont.h + h + 5 + 15;

   /* Standing */
   window_moveWidget( wid, "txtSStanding", x, y );
   window_moveWidget( wid, "txtStanding", x + 50, y - gl_smallFont.h - 5 );
   y -= 2 * gl_smallFont.h + 5 + 15;

   /* Get presence. */
   hasPresence = 0;
   buf[0]      = '\0';
   l           = 0;
   unknownPresence = 0;
   for (i=0; i < sys->npresence; i++) {
      if (sys->presence[i].value <= 0)
         continue;
      hasPresence = 1;
      if (faction_isKnown( sys->presence[i].faction )) {
         t = faction_getColourChar(sys->presence[i].faction);
         /* Use map grey instead of default neutral colour */
         l += nsnprintf( &buf[l], PATH_MAX-l, "%s\a0%s: \a%c%.0f",
                        (l==0)?"":"\n", faction_shortname(sys->presence[i].faction),
                        t, sys->presence[i].value);
      }
      else
         unknownPresence += sys->presence[i].value;
      if (l > PATH_MAX)
         break;
   }
   if (unknownPresence != 0)
      l += nsnprintf( &buf[l], PATH_MAX-l, "%s\a0%s: \a%c%.0f",
                     (l==0)?"":"\n", _("Unknown"), 'N', unknownPresence);
   if (hasPresence == 0)
      nsnprintf(buf, PATH_MAX, "N/A");
   window_moveWidget( wid, "txtSPresence", x, y );
   window_moveWidget( wid, "txtPresence", x + 50, y-gl_smallFont.h-5 );
   window_modifyText( wid, "txtPresence", buf );
   /* Scroll down. */
   h  = gl_printHeightRaw( &gl_smallFont, w, buf );
   y -= 40 + (h - gl_smallFont.h);

   /* Get planets */
   hasPlanets = 0;
   p = 0;
   buf[0] = '\0';
   for (i=0; i<sys->nplanets; i++) {
      if (sys->planets[i]->real != ASSET_REAL)
         continue;
      if (!planet_isKnown(sys->planets[i]))
         continue;

      /* Colourize output. */
      planet_updateLand(sys->planets[i]);
      t = planet_getColourChar(sys->planets[i]);
      sym = planet_getSymbol(sys->planets[i]);

      if (!hasPlanets)
         p += nsnprintf( &buf[p], PATH_MAX-p, "\a%c%s%s\an",
               t, sym, sys->planets[i]->name );
      else
         p += nsnprintf( &buf[p], PATH_MAX-p, ",\n\a%c%s%s\an",
               t, sym, sys->planets[i]->name );
      hasPlanets = 1;
      if (p > PATH_MAX)
         break;
   }
   if (hasPlanets == 0) {
      strncpy( buf, _("None"), PATH_MAX );
      buf[sizeof(buf)-1] = '\0';
   }
   /* Update text. */
   window_modifyText( wid, "txtPlanets", buf );
   window_moveWidget( wid, "txtSPlanets", x, y );
   window_moveWidget( wid, "txtPlanets", x + 50, y-gl_smallFont.h-5 );
   /* Scroll down. */
   h  = gl_printHeightRaw( &gl_smallFont, w, buf );
   y -= 40 + (h - gl_smallFont.h);

   /* Get the services */
   window_moveWidget( wid, "txtSServices", x, y );
   window_moveWidget( wid, "txtServices", x + 50, y-gl_smallFont.h-5 );
   services = 0;
   for (i=0; i<sys->nplanets; i++)
      if (planet_isKnown(sys->planets[i]))
         services |= sys->planets[i]->services;
   buf[0] = '\0';
   p = 0;
   /*nsnprintf(buf, sizeof(buf), "%f\n", sys->prices[0]);*/ /*Hack to control prices. */
   for (i=PLANET_SERVICE_MISSIONS; i<=PLANET_SERVICE_SHIPYARD; i<<=1)
      if (services & i)
         p += nsnprintf( &buf[p], PATH_MAX-p, "%s\n", planet_getServiceName(i) );
   if (buf[0] == '\0')
      p += nsnprintf( &buf[p], PATH_MAX-p, _("None"));
   window_modifyText( wid, "txtServices", buf );


   /*
    * System Status, if not showing commodity info
    */
   if (cur_commod == -1) {
      buf[0] = '\0';
      p = 0;
      /* Nebula. */
      if (sys->nebu_density > 0.) {
         /* Density. */
         if (sys->nebu_density > 700.)
            adj = _("Dense ");
         else if (sys->nebu_density < 300.)
            adj = _("Light ");
         else
            adj = "";

         /* Volatility */
         if (sys->nebu_volatility > 700.)
            p += nsnprintf(&buf[p], PATH_MAX-p, _("Volatile %sNebula"), adj);
         else if (sys->nebu_volatility > 300.)
            p += nsnprintf(&buf[p], PATH_MAX-p, _("Dangerous %sNebula"), adj);
         else if (sys->nebu_volatility > 0.)
            p += nsnprintf(&buf[p], PATH_MAX-p, _("Unstable %sNebula"), adj);
         else
            p += nsnprintf(&buf[p], PATH_MAX-p, _("%sNebula"), adj);
      }
      /* Interference. */
      if (sys->interference > 0.) {
         if (buf[0] != '\0')
            p += nsnprintf(&buf[p], PATH_MAX-p, _(", "));

         if (sys->interference > 700.)
            p += nsnprintf(&buf[p], PATH_MAX-p, _("Dense Interference"));
         else if (sys->interference < 300.)
            p += nsnprintf(&buf[p], PATH_MAX-p, _("Light Interference"));
         else
            p += nsnprintf(&buf[p], PATH_MAX-p, _("Interference"));
      }
      /* Asteroids. */
      if (sys->nasteroids > 0) {
         double density;

         if (buf[0] != '\0')
            p += nsnprintf(&buf[p], PATH_MAX-p, _(", "));

         density = 0.;
         for (i=0; i<sys->nasteroids; i++) {
            density += sys->asteroids[i].area * sys->asteroids[i].density;
         }

         if (density >= 1.5)
            p += nsnprintf(&buf[p], PATH_MAX-p, _("Dense Asteroid Field"));
         else if (density <= 0.5)
            p += nsnprintf(&buf[p], PATH_MAX-p, _("Light Asteroid Field"));
         else
            p += nsnprintf(&buf[p], PATH_MAX-p, _("Asteroid Field"));
      }
      window_modifyText( wid, "txtSystemStatus", buf );
   }
}


/**
 * @brief Checks to see if the map is open.
 *
 *    @return 0 if map is closed, non-zero if it's open.
 */
int map_isOpen (void)
{
   return window_exists(MAP_WDWNAME);
}


/**
 * @brief Draws a mission marker on the map.
 *
 * @param x X position to draw at.
 * @param y Y position to draw at.
 * @param r Radius of system.
 * @param a Colour alpha to use.
 * @param num Total number of markers.
 * @param cur Current marker to draw.
 * @param type Type to draw.
 */
static void map_drawMarker( double x, double y, double r, double a,
      int num, int cur, int type )
{
   static const glColour* colours[] = {
      &cGreen, &cBlue, &cRed, &cOrange, &cYellow
   };

   double alpha;
   glColour col;
   gl_Matrix4 projection;

   /* Calculate the angle. */
   if ((num == 1) || (num == 2) || (num == 4))
      alpha = M_PI/4.;
   else if (num == 3)
      alpha = M_PI/6.;
   else if (num == 5)
      alpha = M_PI/10.;
   else
      alpha = M_PI/2.;

   alpha += M_PI*2. * (double)cur/(double)num;

   /* Draw the marking triangle. */
   glEnable(GL_POLYGON_SMOOTH);
   col = *colours[type];
   col.a *= a;
   projection = gl_Matrix4_Translate(gl_view_matrix, x, y, 0);
   projection = gl_Matrix4_Scale(projection, r, r, 1);
   projection = gl_Matrix4_Rotate2d(projection, alpha);
   gl_beginSolidProgram(projection, &col);
   gl_vboActivateAttribOffset( marker_vbo, shaders.solid.vertex, 0, 2, GL_FLOAT, 0 );
   glDrawArrays( GL_TRIANGLES, 0, 3 );
   gl_endSolidProgram();
   glDisable(GL_POLYGON_SMOOTH);
}

/**
 * @brief Generates a texture to represent factions
 *
 * @param radius radius of the disk
 * @return the texture
 */
static glTexture *gl_genFactionDisk( int radius )
{
   int i, j;
   uint8_t *pixels;
   SDL_Surface *sur;
   int dist;
   double alpha;

   /* Calculate parameters. */
   const int w = 2 * radius + 1;
   const int h = 2 * radius + 1;

   /* Create the surface. */
   sur = SDL_CreateRGBSurface( 0, w, h, 32, RGBAMASK );

   pixels = sur->pixels;
   memset(pixels, 0xff, sizeof(uint8_t) * 4 * h * w);

   /* Generate the circle. */
   SDL_LockSurface( sur );

   /* Draw the circle with filter. */
   for (i=0; i<h; i++) {
      for (j=0; j<w; j++) {
         /* Calculate blur. */
         dist = (i - radius) * (i - radius) + (j - radius) * (j - radius);
         alpha = 0.;

         if (dist < radius * radius) {
            /* Computes alpha with an empirically chosen formula.
             * This formula accounts for the fact that the eyes
             * has a logarithmic sensitivity to light */
            alpha = 1. * dist / (radius * radius);
            alpha = (exp(1 / (alpha + 1) - 0.5) - 1) * 0xFF;
         }

         /* Sets the pixel alpha which is the forth byte
          * in the pixel representation. */
         pixels[i*sur->pitch + j*4 + 3] = (uint8_t)alpha;
      }
   }

   SDL_UnlockSurface( sur );

   /* Return texture. */
   return gl_loadImage( sur, OPENGL_TEX_MIPMAPS );
}

/**
 * @brief Renders the custom map widget.
 *
 *    @param bx Base X position to render at.
 *    @param by Base Y position to render at.
 *    @param w Width of the widget.
 *    @param h Height of the widget.
 */
static void map_render( double bx, double by, double w, double h, void *data )
{
   (void) data;
   double x,y,r;
   glColour col;
   StarSystem *sys;
   if ( commod_counter > 0 )
      commod_counter--;
   /* Parameters. */
   map_renderParams( bx, by, map_xpos, map_ypos, w, h, map_zoom, &x, &y, &r );

   /* background */
   gl_renderRect( bx, by, w, h, &cBlack );

   if ( cur_commod == -1 )
      map_renderDecorators( x, y, 0 );

   /* Render faction disks. */
   if ( cur_commod == -1 )
      map_renderFactionDisks( x, y, 0 );

   /* Render jump routes. */
   map_renderJumps( x, y, 0 );

   /* Cause alpha to move smoothly between 0-1 every second. */
   col.a = ABS( 500 - (int)SDL_GetTicks() % 1000 ) / 500.;

   /* Render the player's jump route. */
   if ( cur_commod == -1 )
      map_renderPath( x, y, col.a );

   /* Render systems. */
   map_renderSystems( bx, by, x, y, w, h, r, 0 );

   /* Render system names. */
   map_renderNames( bx, by, x, y, w, h, 0 );

   /* Render system markers. */
   if ( cur_commod == -1 )
     map_renderMarkers( x, y, r, col.a );

   /* Render commodity info. */
   map_renderCommod(  bx, by, x, y, w, h, r, 0 );

   /* Initialize with values from cRed */
   col.r = cRed.r;
   col.g = cRed.g;
   col.b = cRed.b;

   /* Selected system. */
   if (map_selected != -1) {
      sys = system_getIndex( map_selected );
      gl_drawCircle( x + sys->pos.x * map_zoom, y + sys->pos.y * map_zoom,
            1.5*r, &col, 0 );
   }

   /* Values from cRadar_tPlanet */
   col.r = cRadar_tPlanet.r;
   col.g = cRadar_tPlanet.g;
   col.b = cRadar_tPlanet.b;

   /* Current planet. */
   gl_drawCircle( x + cur_system->pos.x * map_zoom,
         y + cur_system->pos.y * map_zoom,
         1.5*r, &col, 0 );
}


/**
 * @brief Gets the render parameters.
 */
void map_renderParams( double bx, double by, double xpos, double ypos,
      double w, double h, double zoom, double *x, double *y, double *r )
{
   *r = round(CLAMP(6., 20., 8.*zoom));
   *x = round((bx - xpos + w/2) * 1.);
   *y = round((by - ypos + h/2) * 1.);
}

/**
 * @brief Renders the map background decorators.
 */
void map_renderDecorators( double x, double y, int editor)
{
   int i,j;
   int sw, sh;
   double tx, ty;
   int visible;
   MapDecorator *decorator;
   StarSystem *sys;
   glColour ccol = { .r=1.00, .g=1.00, .b=1.00, .a=1. }; /**< White */

   /* Fade in the decorators to allow toggling between commodity and nothing */
   double cc = cos ( commod_counter / 200. * M_PI );
   ccol.a = 2./3.*cc;

   for (i=0; i<decorator_nstack; i++) {

      decorator = &decorator_stack[i];

      /* only if pict couldn't be loaded */
      if (decorator->image == NULL)
         continue;

      visible=0;

      if (!editor) {
         for (j=0; j<systems_nstack && visible==0; j++) {
            sys = system_getIndex( j );

            if (!sys_isKnown(sys))
               continue;

            if ((decorator->x < sys->pos.x + decorator->detection_radius) &&
                  (decorator->x > sys->pos.x - decorator->detection_radius) &&
                  (decorator->y < sys->pos.y + decorator->detection_radius) &&
                  (decorator->y > sys->pos.y - decorator->detection_radius)) {
               visible=1;
            }
         }
      }

      if (editor || visible==1) {

         tx = x + decorator->x*map_zoom;
         ty = y + decorator->y*map_zoom;

         sw = decorator->image->sw*map_zoom;
         sh = decorator->image->sh*map_zoom;

         gl_blitScale(
               decorator->image,
               tx - sw/2, ty - sh/2, sw, sh, &ccol );
      }
   }
}


/**
 * @brief Renders the faction disks.
 */
void map_renderFactionDisks( double x, double y, int editor)
{
   int i;
   const glColour *col;
   glColour c;
   StarSystem *sys;
   int sw, sh;
   double tx, ty, presence;
   /* Fade in the disks to allow toggling between commodity and nothing */
   double cc = cos ( commod_counter / 200. * M_PI );

   for (i=0; i<systems_nstack; i++) {
      sys = system_getIndex( i );

      /* System has no faction, or isn't known and we aren't in the editor. */
      if (sys->faction == -1 || (!sys_isKnown(sys) && !editor))
         continue;

      tx = x + sys->pos.x*map_zoom;
      ty = y + sys->pos.y*map_zoom;

      /* Cache to avoid repeated sqrt() */
      presence = sqrt(sys->ownerpresence);

      /* draws the disk representing the faction */
      sw = (60 + presence * 3) * map_zoom;
      sh = (60 + presence * 3) * map_zoom;

      col = faction_colour(sys->faction);
      c.r = col->r;
      c.g = col->g;
      c.b = col->b;
      //c.a = CLAMP( .6, .75, 20 / presence ) * cc;
      c.a = CLAMP( .4, .5, 13.3 / presence ) * cc;

      gl_blitTexture(
            gl_faction_disk,
            tx - sw/2, ty - sh/2, sw, sh,
            0., 0., gl_faction_disk->srw, gl_faction_disk->srw, &c );
   }
}


/**
 * @brief Renders the jump routes between systems.
 */
void map_renderJumps( double x, double y, int editor)
{
   int i, j, k;
   const glColour *col, *cole;
   GLfloat vertex[8*(2+4)];
   StarSystem *sys, *jsys;

   /* Generate smooth lines. */
   glLineWidth( CLAMP(1., 4., 2. * map_zoom)*gl_screen.scale );

   for (i=0; i<systems_nstack; i++) {
      sys = system_getIndex( i );

      if (!sys_isKnown(sys) && !editor)
         continue; /* we don't draw hyperspace lines */

      /* first we draw all of the paths. */
      gl_beginSmoothProgram(gl_view_matrix);
      gl_vboActivateAttribOffset( map_vbo, shaders.smooth.vertex, 0, 2, GL_FLOAT, 0 );
      gl_vboActivateAttribOffset( map_vbo, shaders.smooth.vertex_color,
            sizeof(GLfloat) * 2*3, 4, GL_FLOAT, 0 );
      for (j = 0; j < sys->njumps; j++) {
         jsys = sys->jumps[j].target;
         if (!space_sysReachableFromSys(jsys,sys) && !editor)
            continue;

         /* Choose colours. */
         cole = &cLightBlue;
         for (k = 0; k < jsys->njumps; k++) {
            if (jsys->jumps[k].target == sys) {
               if (jp_isFlag(&jsys->jumps[k], JP_EXITONLY))
                  cole = &cWhite;
               else if (jp_isFlag(&jsys->jumps[k], JP_HIDDEN))
                  cole = &cRed;
               break;
            }
         }
         if (jp_isFlag(&sys->jumps[j], JP_EXITONLY))
            col = &cWhite;
         else if (jp_isFlag(&sys->jumps[j], JP_HIDDEN))
            col = &cRed;
         else
            col = &cLightBlue;

         /* Draw the lines. */
         vertex[0]  = x + sys->pos.x * map_zoom;
         vertex[1]  = y + sys->pos.y * map_zoom;
         vertex[2]  = vertex[0] + (jsys->pos.x - sys->pos.x)/2. * map_zoom;
         vertex[3]  = vertex[1] + (jsys->pos.y - sys->pos.y)/2. * map_zoom;
         vertex[4]  = x + jsys->pos.x * map_zoom;
         vertex[5]  = y + jsys->pos.y * map_zoom;
         vertex[6]  = col->r;
         vertex[7]  = col->g;
         vertex[8]  = col->b;
         vertex[9]  = 0.2;
         vertex[10] = (col->r + cole->r)/2.;
         vertex[11] = (col->g + cole->g)/2.;
         vertex[12] = (col->b + cole->b)/2.;
         vertex[13] = 0.8;
         vertex[14] = cole->r;
         vertex[15] = cole->g;
         vertex[16] = cole->b;
         vertex[17] = 0.2;
         gl_vboSubData( map_vbo, 0, sizeof(GLfloat) * 3*(2+4), vertex );
         glDrawArrays( GL_LINE_STRIP, 0, 3 );
      }
      gl_endSmoothProgram();
   }

   /* Reset render parameters. */
   glLineWidth( 1. );
}


/**
 * @brief Renders the systems.
 */
void map_renderSystems( double bx, double by, double x, double y,
      double w, double h, double r, int editor)
{
   int i;
   const glColour *col;
   StarSystem *sys;
   double tx, ty;

   for (i=0; i<systems_nstack; i++) {
      sys = system_getIndex( i );

      /* if system is not known, reachable, or marked. and we are not in the editor */
      if ((!sys_isKnown(sys) && !sys_isFlag(sys, SYSTEM_MARKED | SYSTEM_CMARKED)
           && !space_sysReachable(sys)) && !editor)
         continue;

      tx = x + sys->pos.x*map_zoom;
      ty = y + sys->pos.y*map_zoom;

      /* Skip if out of bounds. */
      if (!rectOverlap(tx - r, ty - r, r, r, bx, by, w, h))
         continue;

      /* Draw an outer ring. */
      gl_drawCircle( tx, ty, r, &cInert, 0 );

      /* If system is known fill it. */
      if ((editor || sys_isKnown(sys)) && (system_hasPlanet(sys))) {
         /* Planet colours */
         if (!editor && !sys_isKnown(sys)) col = &cInert;
         else if (sys->faction < 0) col = &cInert;
         else if (editor) col = &cNeutral;
         else col = faction_getColour( sys->faction );

         if (editor) {
            /* Radius slightly shorter. */
            gl_drawCircle( tx, ty, 0.5 * r, col, 1 );
         }
         else
            gl_drawCircle( tx, ty, 0.65 * r, col, 1 );
      }

   }
}


/**
 * @brief Render the map path.
 */
static void map_renderPath( double x, double y, double a )
{
   int j;
   const glColour *col;
   GLfloat vertex[8*(2+4)];
   StarSystem *jsys, *lsys;
   int jmax, jcur;

   if (map_path != NULL) {
      lsys = cur_system;
      jmax = pilot_getJumps(player.p); /* Maximum jumps. */
      jcur = jmax; /* Jump range remaining. */

      /* Generate smooth lines. */
      glLineWidth( CLAMP(1., 4., 2. * map_zoom)*gl_screen.scale );

      for (j=0; j<map_npath; j++) {
         jsys = map_path[j];
         if (jcur == jmax && jmax > 0)
            col = &cGreen;
         else if (jcur < 1)
            col = &cRed;
         else
            col = &cYellow;
         jcur--;

         /* Draw the lines. */
         vertex[0]  = x + lsys->pos.x * map_zoom;
         vertex[1]  = y + lsys->pos.y * map_zoom;
         vertex[2]  = vertex[0] + (jsys->pos.x - lsys->pos.x)/2. * map_zoom;
         vertex[3]  = vertex[1] + (jsys->pos.y - lsys->pos.y)/2. * map_zoom;
         vertex[4]  = x + jsys->pos.x * map_zoom;
         vertex[5]  = y + jsys->pos.y * map_zoom;
         vertex[6]  = col->r;
         vertex[7]  = col->g;
         vertex[8]  = col->b;
         vertex[9]  = a / 4. + .25;
         vertex[10] = col->r;
         vertex[11] = col->g;
         vertex[12] = col->b;
         vertex[13] = a / 2. + .5;
         vertex[14] = col->r;
         vertex[15] = col->g;
         vertex[16] = col->b;
         vertex[17] = a / 4. + .25;
         gl_vboSubData( map_vbo, 0, sizeof(GLfloat) * 3*(2+4), vertex );

         gl_beginSmoothProgram(gl_view_matrix);
         gl_vboActivateAttribOffset( map_vbo, shaders.smooth.vertex, 0, 2, GL_FLOAT, 0 );
         gl_vboActivateAttribOffset( map_vbo, shaders.smooth.vertex_color,
               sizeof(GLfloat) * 2*3, 4, GL_FLOAT, 0 );
         glDrawArrays( GL_LINE_STRIP, 0, 3 );
         gl_endSmoothProgram();

         lsys = jsys;
      }

      /* Reset render parameters. */
      glLineWidth( 1. );
   }
}


/**
 * @brief Renders the system names on the map.
 */
void map_renderNames( double bx, double by, double x, double y,
      double w, double h, int editor )
{
   double tx,ty, vx,vy, d,n;
   int textw;
   StarSystem *sys, *jsys;
   int i, j;
   char buf[32];

   for (i=0; i<systems_nstack; i++) {
      sys = system_getIndex( i );

      /* Skip system. */
      if ((!editor && !sys_isKnown(sys)) || (map_zoom <= 0.5 ))
         continue;

      textw = gl_printWidthRaw( &gl_smallFont, sys->name );
      tx = x + (sys->pos.x+11.) * map_zoom;
      ty = y + (sys->pos.y-5.) * map_zoom;

      /* Skip if out of bounds. */
      if (!rectOverlap(tx, ty, textw, gl_smallFont.h, bx, by, w, h))
         continue;

      gl_print( &gl_smallFont,
            tx, ty,
            &cWhite, sys->name );

   }

   /* Raw hidden values if we're in the editor. */
   if (!editor || (map_zoom <= 1.0))
      return;

   for (i=0; i<systems_nstack; i++) {
      sys = system_getIndex( i );
      for (j=0; j<sys->njumps; j++) {
         jsys = sys->jumps[j].target;
         /* Calculate offset. */
         vx  = jsys->pos.x - sys->pos.x;
         vy  = jsys->pos.y - sys->pos.y;
         n   = sqrt( pow2(vx) + pow2(vy) );
         vx /= n;
         vy /= n;
         d   = MAX(n*0.3*map_zoom, 15);
         tx  = x + map_zoom*sys->pos.x + d*vx;
         ty  = y + map_zoom*sys->pos.y + d*vy;
         /* Display. */
         n = sqrt(sys->jumps[j].hide);
         if (n == 0.)
            nsnprintf( buf, sizeof(buf), "\agH: %.2f", n );
         else
            nsnprintf( buf, sizeof(buf), "H: %.2f", n );
         gl_print( &gl_smallFont,
               tx, ty,
               &cGrey70, buf );
      }
   }
}


/**
 * @brief Renders the map markers.
 */
static void map_renderMarkers( double x, double y, double r, double a )
{
   double tx, ty;
   int i, j, n, m;
   StarSystem *sys;

   for (i=0; i<systems_nstack; i++) {
      sys = system_getIndex( i );

      /* We only care about marked now. */
      if (!sys_isFlag(sys, SYSTEM_MARKED | SYSTEM_CMARKED))
         continue;

      /* Get the position. */
      tx = x + sys->pos.x*map_zoom;
      ty = y + sys->pos.y*map_zoom;

      /* Count markers. */
      n  = (sys_isFlag(sys, SYSTEM_CMARKED)) ? 1 : 0;
      n += sys->markers_plot;
      n += sys->markers_high;
      n += sys->markers_low;
      n += sys->markers_computer;

      /* Draw the markers. */
      j = 0;
      if (sys_isFlag(sys, SYSTEM_CMARKED)) {
         map_drawMarker( tx, ty, r, a, n, j, 0 );
         j++;
      }
      for (m=0; m<sys->markers_plot; m++) {
         map_drawMarker( tx, ty, r, a, n, j, 1 );
         j++;
      }
      for (m=0; m<sys->markers_high; m++) {
         map_drawMarker( tx, ty, r, a, n, j, 2 );
         j++;
      }
      for (m=0; m<sys->markers_low; m++) {
         map_drawMarker( tx, ty, r, a, n, j, 3 );
         j++;
      }
      for (m=0; m<sys->markers_computer; m++) {
         map_drawMarker( tx, ty, r, a, n, j, 4 );
         j++;
      }
   }
}

#define setcolour(R,G,B) ({ccol.r=(R);ccol.g=(G);ccol.b=(B);ccol.a=1;})
/*
 * Makes all systems dark grey.
 */
static void map_renderSysBlack(double bx, double by, double x,double y, double w, double h, double r, int editor)
{
   int i;
   StarSystem *sys;
   double tx,ty;
   glColour ccol;

   for (i=0; i<systems_nstack; i++) {
      sys = system_getIndex( i );

      /* if system is not known, reachable, or marked. and we are not in the editor */
      if ((!sys_isKnown(sys) && !sys_isFlag(sys, SYSTEM_MARKED | SYSTEM_CMARKED)
           && !space_sysReachable(sys)) && !editor)
         continue;

      tx = x + sys->pos.x*map_zoom;
      ty = y + sys->pos.y*map_zoom;

      /* Skip if out of bounds. */
      if (!rectOverlap(tx - r, ty - r, r, r, bx, by, w, h))
         continue;

      /* If system is known fill it. */
      if ((sys_isKnown(sys)) && (system_hasPlanet(sys))) {
         setcolour(0.1,0.1,0.1);
         gl_drawCircle( tx, ty , r, &ccol, 1 );
      }
   }
}


/*
 * Renders the economy information
 */

void map_renderCommod( double bx, double by, double x, double y,
      double w, double h, double r, int editor)
{
   int i,j,k;
   StarSystem *sys;
   double tx, ty;
   Planet *p;
   Commodity *c;
   glColour ccol;
   double best,worst,maxPrice,minPrice,curMaxPrice,curMinPrice,thisPrice;
   char buf[80];
   int textw;
   /* If not plotting commodities, return */
   if (cur_commod == -1 || map_selected == -1)
      return;

   c=commod_known[cur_commod];
   if ( cur_commod_mode == 0 ) {/*showing price difference to selected system*/
     /* Get commodity price in selected system.  If selected system is current
        system, and if landed, then get price of commodity where we are */
      curMaxPrice=0.;
      curMinPrice=0.;
      sys = system_getIndex( map_selected );
      if ( sys == cur_system && landed ) {
         for ( k=0; k<land_planet->ncommodities; k++ ) {
            if ( land_planet->commodities[k] == c ) {
               /* current planet has the commodity of interest */
               curMinPrice = land_planet->commodityPrice[k].sum / land_planet->commodityPrice[k].cnt;
               curMaxPrice = curMinPrice;
               break;
            }
         }
         if ( k == land_planet->ncommodities ) { /* commodity of interest not found */
            textw = gl_printWidthRaw( &gl_smallFont, _("No price info for") );
            gl_print( &gl_smallFont,x + sys->pos.x *map_zoom- textw/2, y + (sys->pos.y+10)*map_zoom, &cRed, _("No price info for"));
            snprintf(buf,80,"%s here",c->name);
            textw = gl_printWidthRaw( &gl_smallFont, buf);
            gl_print( &gl_smallFont,x + sys->pos.x *map_zoom- textw/2, y + (sys->pos.y-15)*map_zoom, &cRed, buf);
            map_renderSysBlack(bx,by,x,y,w,h,r,editor);
            return;
         }
      } else {
         /* not currently landed, so get max and min price in the selected system. */
         if ((sys_isKnown(sys)) && (system_hasPlanet(sys))) {
            minPrice=0;
            maxPrice=0;
            for ( j=0 ; j<sys->nplanets; j++) {
               p=sys->planets[j];
               for ( k=0; k<p->ncommodities; k++) {
                  if ( p->commodities[k] == c ) {
                     if ( p->commodityPrice[k].cnt > 0 ) {/*commodity is known about*/
                        thisPrice = p->commodityPrice[k].sum / p->commodityPrice[k].cnt;
                        if (thisPrice > maxPrice)maxPrice=thisPrice;
                        if (minPrice == 0 || thisPrice < minPrice)minPrice = thisPrice;
                        break;
                     }
                  }
               }

            }
            if ( maxPrice == 0 ) {/* no prices are known here */
               textw = gl_printWidthRaw( &gl_smallFont, _("No price info for") );
               gl_print( &gl_smallFont,x + sys->pos.x *map_zoom- textw/2, y + (sys->pos.y+10)*map_zoom, &cRed, _("No price info for"));
               snprintf(buf,80,"%s here",c->name);
               textw = gl_printWidthRaw( &gl_smallFont, buf);
               gl_print( &gl_smallFont,x + sys->pos.x *map_zoom- textw/2, y + (sys->pos.y-15)*map_zoom, &cRed, buf);
               map_renderSysBlack(bx,by,x,y,w,h,r,editor);
               return;

            }
            curMaxPrice=maxPrice;
            curMinPrice=minPrice;
         } else {
            textw = gl_printWidthRaw( &gl_smallFont, _("No price info for") );
            gl_print( &gl_smallFont,x + sys->pos.x *map_zoom- textw/2, y + (sys->pos.y+10)*map_zoom, &cRed, _("No price info for"));
            snprintf(buf,80,"%s here",c->name);
            textw = gl_printWidthRaw( &gl_smallFont, buf);
            gl_print( &gl_smallFont,x + sys->pos.x *map_zoom- textw/2, y + (sys->pos.y-15)*map_zoom, &cRed, buf);
            map_renderSysBlack(bx,by,x,y,w,h,r,editor);
            return;
         }
      }
      for (i=0; i<systems_nstack; i++) {
         sys = system_getIndex( i );

         /* if system is not known, reachable, or marked. and we are not in the editor */
         if ((!sys_isKnown(sys) && !sys_isFlag(sys, SYSTEM_MARKED | SYSTEM_CMARKED)
              && !space_sysReachable(sys)) && !editor)
            continue;

         tx = x + sys->pos.x*map_zoom;
         ty = y + sys->pos.y*map_zoom;

         /* Skip if out of bounds. */
         if (!rectOverlap(tx - r, ty - r, r, r, bx, by, w, h))
            continue;

         /* If system is known fill it. */
         if ((sys_isKnown(sys)) && (system_hasPlanet(sys))) {
            minPrice=0;
            maxPrice=0;
            for ( j=0 ; j<sys->nplanets; j++) {
               p=sys->planets[j];
               for ( k=0; k<p->ncommodities; k++) {
                  if ( p->commodities[k] == c ) {
                     if ( p->commodityPrice[k].cnt > 0 ) {/*commodity is known about*/
                        thisPrice = p->commodityPrice[k].sum / p->commodityPrice[k].cnt;
                        if (thisPrice > maxPrice)maxPrice=thisPrice;
                        if (minPrice == 0 || thisPrice < minPrice)minPrice = thisPrice;
                        break;
                     }
                  }
               }
            }


            /* Calculate best and worst profits */
            if ( maxPrice > 0 ) {
               /* Commodity sold at this system */
               best = maxPrice - curMinPrice ;
               worst= minPrice - curMaxPrice ;
               if ( best >= 0 ) {/* draw circle above */
                  gl_print(&gl_smallFont, x + (sys->pos.x+11) * map_zoom , y + (sys->pos.y-22)*map_zoom, &cLightBlue, "%.1f",best);
                  best = tanh ( 2*best / curMinPrice );
                  setcolour(1-best,1-best,best);/*yellow (0) to blue (1)*/
                  gl_drawCircle( tx, ty /*+ r*/ , /*(0.1 + best) **/ r, &ccol, 1 );
               } else {/* draw circle below */
                  gl_print(&gl_smallFont, x + (sys->pos.x+11) * map_zoom , y + (sys->pos.y-22)*map_zoom, &cOrange, "%.1f",worst);
                  worst= tanh ( -2*worst/ curMaxPrice );
                  setcolour(1,1-worst/2,0);/*yellow (0) to orange (1)*/
                  gl_drawCircle( tx, ty /*- r*/ , /*(0.1 - worst) **/ r, &ccol, 1 );
               }
            } else {
               /* Commodity not sold here */
               setcolour(0.1,0.1,0.1);
               gl_drawCircle( tx, ty , r, &ccol, 1 );

            }
         }
      }
   } else { /* cur_commod_mode == 1, showing actual prices */
      /*First calculate av price in all systems */
      /* This has already been done in map_update_commod_av_price */
      /* Now display the costs */
      for (i=0; i<systems_nstack; i++) {
         sys = system_getIndex( i );

         /* if system is not known, reachable, or marked. and we are not in the editor */
         if ((!sys_isKnown(sys) && !sys_isFlag(sys, SYSTEM_MARKED | SYSTEM_CMARKED)
              && !space_sysReachable(sys)) && !editor)
            continue;

         tx = x + sys->pos.x*map_zoom;
         ty = y + sys->pos.y*map_zoom;

         /* Skip if out of bounds. */
         if (!rectOverlap(tx - r, ty - r, r, r, bx, by, w, h))
            continue;

         /* If system is known fill it. */
         if ((sys_isKnown(sys)) && (system_hasPlanet(sys))) {
            double sumPrice=0;
            int sumCnt=0;
            for ( j=0 ; j<sys->nplanets; j++) {
               p=sys->planets[j];
               for ( k=0; k<p->ncommodities; k++) {
                  if ( p->commodities[k] == c ) {
                     if ( p->commodityPrice[k].cnt > 0 ) {/*commodity is known about*/
                        thisPrice = p->commodityPrice[k].sum / p->commodityPrice[k].cnt;
                        sumPrice+=thisPrice;
                        sumCnt+=1;
                        break;
                     }
                  }
               }
            }

            if ( sumCnt > 0 ) {
               /* Commodity sold at this system */
               /* Colour as a % of global average */
               double frac;
               sumPrice/=sumCnt;
               if ( sumPrice < commod_av_gal_price ) {
                  frac = tanh(5*(commod_av_gal_price / sumPrice - 1));
                  setcolour(1,1-frac/2,0);/*orange(1) to yellow(0)*/
               } else {
                  frac = tanh(5*(sumPrice / commod_av_gal_price - 1));
                  setcolour(1-frac,1-frac,frac);/*yellow (0) to blue (1)*/
               }
               gl_print(&gl_smallFont, x + (sys->pos.x+11) * map_zoom , y + (sys->pos.y-22)*map_zoom, &ccol, "%.1f",sumPrice);
               gl_drawCircle( tx, ty , r, &ccol, 1 );
            } else {
               /* Commodity not sold here */
               setcolour(0.1,0.1,0.1);
               gl_drawCircle( tx, ty , r, &ccol, 1 );
            }
         }
      }
   }
}
#undef setcolour

/**
 * @brief Map custom widget mouse handling.
 *
 *    @param wid Window sending events.
 *    @param event Event window is sending.
 *    @param mx Mouse X position.
 *    @param my Mouse Y position.
 *    @param w Width of the widget.
 *    @param h Height of the widget.
 */
static int map_mouse( unsigned int wid, SDL_Event* event, double mx, double my,
      double w, double h, void *data )
{
   (void) wid;
   (void) data;
   int i;
   double x,y, t;
   StarSystem *sys;

   t = 15.*15.; /* threshold */

   switch (event->type) {
   case SDL_MOUSEWHEEL:
      /* Must be in bounds. */
      if ((mx < 0.) || (mx > w) || (my < 0.) || (my > h))
         return 0;
      /*if ( listMapModeVisible == 0 ) {*/
      if (event->wheel.y > 0)
         map_buttonZoom( 0, "btnZoomIn" );
      else
         map_buttonZoom( 0, "btnZoomOut" );
      /*}*/
      return 1;

   case SDL_MOUSEBUTTONDOWN:
      /* Must be in bounds. */
      if ((mx < 0.) || (mx > w) || (my < 0.) || (my > h))
         return 0;

      /* selecting star system */
      else {
         mx -= w/2 - map_xpos;
         my -= h/2 - map_ypos;
         map_drag = 1;

         for (i=0; i<systems_nstack; i++) {
            sys = system_getIndex( i );

            /* must be reachable */
            if (!sys_isFlag(sys, SYSTEM_MARKED | SYSTEM_CMARKED)
                && !space_sysReachable(sys))
               continue;

            /* get position */
            x = sys->pos.x * map_zoom;
            y = sys->pos.y * map_zoom;

            if ((pow2(mx-x)+pow2(my-y)) < t) {
               if (map_selected != -1) {
                  if (sys == system_getIndex( map_selected ) && sys_isKnown(sys)) {
                     map_system_open( map_selected );
                     map_drag = 0;
                  }
               }
               map_select( sys, (SDL_GetModState() & KMOD_SHIFT) );
               break;
            }
         }
      }
      return 1;

   case SDL_MOUSEBUTTONUP:
      if (map_drag)
         map_drag = 0;
      break;

   case SDL_MOUSEMOTION:
      if (map_drag) {
         /* axis is inverted */
         map_xpos -= event->motion.xrel;
         map_ypos += event->motion.yrel;
      }
      break;
   }

   return 0;
}
/**
 * @brief Handles the button zoom clicks.
 *
 *    @param wid Unused.
 *    @param str Name of the button creating the event.
 */
static void map_buttonZoom( unsigned int wid, char* str )
{
   (void) wid;

   /* Transform coords to normal. */
   map_xpos /= map_zoom;
   map_ypos /= map_zoom;

   /* Apply zoom. */
   if (strcmp(str,"btnZoomIn")==0) {
      map_zoom *= 1.2;
      map_zoom = MIN(2.5, map_zoom);
   }
   else if (strcmp(str,"btnZoomOut")==0) {
      map_zoom *= 0.8;
      map_zoom = MAX(0.5, map_zoom);
   }

   map_setZoom(map_zoom);

   /* Transform coords back. */
   map_xpos *= map_zoom;
   map_ypos *= map_zoom;
}

/**
 * @brief Generates the list of map modes, i.e. commodities that have been seen so far.
 */
static void map_genModeList(void)
{
   int i,j,k,l;
   int tot=0;
   Planet *p;
   StarSystem *sys;
   int totGot = 0;
   if ( commod_known == NULL )
      commod_known = malloc(sizeof(Commodity*) * commodity_getN());
   memset(commod_known,0,sizeof(Commodity*)*commodity_getN());
   for (i=0; i<systems_nstack; i++) {
      sys = system_getIndex( i );
      for ( j=0 ; j<sys->nplanets; j++) {
         p=sys->planets[j];
         tot+=p->ncommodities;
         for ( k=0; k<p->ncommodities; k++) {
            if ( p->commodityPrice[k].cnt > 0 ) {/*commodity is known about*/
               /* find out which commodity this is */
               for ( l=0 ; l<totGot; l++) {
                  if ( p->commodities[k] == commod_known[l] )
                     break;
               }
               if ( l == totGot ) {
                  commod_known[totGot] = p->commodities[k];
                  totGot++;
               }

            }
         }
      }
   }
   if ( map_modes != NULL ) {
      for ( i=0 ; i<nmap_modes ; i++)
         free( map_modes[i] );
      free ( map_modes );
   }
   nmap_modes = 2*totGot + 1;
   map_modes = calloc( sizeof(char*), nmap_modes );
   map_modes[0] = strdup("Travel (Default)");

   for ( i=0; i<totGot; i++ ) {
      l = strlen(commod_known[i]->name) + 7;
      map_modes[ 2*i + 1 ] = malloc(l);
      nsnprintf(map_modes[2*i+1],l, "%s: Cost", commod_known[i]->name );
      l+=6;
      map_modes[ 2*i + 2 ] = malloc(l);
      nsnprintf(map_modes[2*i+2],l, "%s: Trade", commod_known[i]->name );
   }
}

/**
 * @brief Updates the map mode list.  This is called when the map update list is clicked.
 *    Unfortunately, also called when scrolled.
 *    @param wid Window of the map window.
 *    @param str Unused.
 */
static void map_modeUpdate( unsigned int wid, char* str )
{
  (void)str;
  int listpos;
   listpos=toolkit_getListPos( wid, "lstMapMode" );
   if ( listMapModeVisible==2) {
      listMapModeVisible=1;
   } else if ( listMapModeVisible == 1 ) {
      if ( listpos == 0) {
         cur_commod = -1;
         cur_commod_mode = 0;
      } else {
         cur_commod = (listpos - 1 ) / 2;
         cur_commod_mode = listpos % 2 ; /* if 1, showing cost, if 0 showing difference */
      }
   }
   if ( cur_commod == -1 )
      commod_counter = 101;
   map_update(wid);

}

/**
 * @brief Handles the button commodity clicks.
 *
 *    @param wid Window widget.
 *    @param str Name of the button creating the event.
 */
static void map_buttonCommodity( unsigned int wid, char* str )
{
   (void)str;
   SDL_Keymod mods;
   char **this_map_modes;
   static int cur_commod_last = 0;
   static int cur_commod_mode_last = 0;
   int defpos;
   /* Clicking the mode button - by default will show (or remove) the list of map modes.
      If ctrl is pressed, will toggle between current mode and default */
   mods = SDL_GetModState();
   if (mods & (KMOD_LCTRL | KMOD_RCTRL)) {/* toggle on/off */
      if ( cur_commod == -1 ) {
         cur_commod = cur_commod_last;
         if ( cur_commod == -1 )
            cur_commod = 0;
         cur_commod_mode = cur_commod_mode_last;
      } else {
         cur_commod_last = cur_commod;
         cur_commod_mode_last = cur_commod_mode;
         cur_commod = -1;
      }
      if ( cur_commod >= (nmap_modes-1)/2 )
         cur_commod = -1;
      /* And hide the list if it was visible. */
      if ( listMapModeVisible) {
         listMapModeVisible = 0;
         window_destroyWidget( wid, "lstMapMode" );
      }
      if ( cur_commod == -1 )
         commod_counter = 101;
      map_update(wid);
   } else {/* no keyboard modifier */
      if ( listMapModeVisible) {/* Hide the list widget */
         listMapModeVisible = 0;
         window_destroyWidget( wid, "lstMapMode" );
      } else {/* show the list widget */
         this_map_modes = calloc( sizeof(char*), nmap_modes );
         for (int i=0; i<nmap_modes;i++) {
            this_map_modes[i]=strdup(map_modes[i]);
         }
         listMapModeVisible = 2;
         if ( cur_commod == -1 )
            defpos = 0;
         else
            defpos = cur_commod*2 + 2 - cur_commod_mode;

         window_addList( wid, -10, 60, 200, 200,
                         "lstMapMode", this_map_modes, nmap_modes, defpos, map_modeUpdate );
      }
   }
}


/**
 * @brief Cleans up the map stuff.
 */
static void map_window_close( unsigned int wid, char *str )
{
   int i;
   if ( commod_known != NULL )
      free ( commod_known );
   commod_known = NULL;
   if ( map_modes != NULL ) {
      for ( i=0; i<nmap_modes; i++ )
         free ( map_modes[i] );
      free ( map_modes );
      map_modes = NULL;
   }
   cur_commod = -1;
   window_close(wid,str);
}

void map_cleanup (void)
{
   map_close();
   map_clear();
}


/**
 * @brief Closes the map.
 */
void map_close (void)
{
   unsigned int wid;

   wid = window_get(MAP_WDWNAME);
   if (wid > 0)
      window_destroy(wid);
}


/**
 * @brief Sets the map to safe defaults
 */
void map_clear (void)
{
   map_setZoom(1.);
   cur_commod = -1;
   if (cur_system != NULL) {
      map_xpos = cur_system->pos.x;
      map_ypos = cur_system->pos.y;
   }
   else {
      map_xpos = 0.;
      map_ypos = 0.;
   }
   if (map_path != NULL) {
      free(map_path);
      map_path = NULL;
      map_npath = 0;
   }

   /* default system is current system */
   map_selectCur();
}


/**
 * @brief Tries to select the current system.
 */
static void map_selectCur (void)
{
   if (cur_system != NULL)
      map_selected = cur_system - systems_stack;
   else
      /* will probably segfault now */
      map_selected = -1;
}


/**
 * @brief Gets the destination system.
 *
 *    @param[out] jumps Number of jumps until the destination.
 *    @return The destination system or NULL if there is no path set.
 */
StarSystem* map_getDestination( int *jumps )
{
   if (map_path == NULL)
      return NULL;

   if (jumps != NULL)
      *jumps = map_npath;

   return map_path[ map_npath-1 ];
}


/**
 * @brief Updates the map after a jump.
 */
void map_jump (void)
{
   int j;

   /* set selected system to self */
   map_selectCur();

   map_xpos = cur_system->pos.x;
   map_ypos = cur_system->pos.y;

   /* update path if set */
   if (map_path != NULL) {
      map_npath--;
      if (map_npath == 0) { /* path is empty */
         free( map_path );
         map_path = NULL;
         player_targetHyperspaceSet( -1 );
      }
      else { /* get rid of bottom of the path */
         memmove( &map_path[0], &map_path[1], sizeof(StarSystem*) * map_npath );

         /* set the next jump to be to the next in path */
         for (j=0; j<cur_system->njumps; j++) {
            if (map_path[0] == cur_system->jumps[j].target) {
               /* Restore selected system. */
               map_selected = map_path[ map_npath ] - systems_stack;

               player_targetHyperspaceSet( j );
               break;
            }
         }
         /* Overrode jump route manually, must clear target. */
         if (j>=cur_system->njumps)
            player_targetHyperspaceSet( -1 );
      }
   }
   else
      player_targetHyperspaceSet( -1 );

   gui_setNav();
}


/**
 * @brief Selects the system in the map.
 *
 *    @param sys System to select.
 */
void map_select( StarSystem *sys, char shifted )
{
   unsigned int wid;
   int i;

   wid = window_get(MAP_WDWNAME);

   if (sys == NULL) {
      map_selectCur();
      window_disableButton( wid, "btnAutonav" );
   }
   else {
      map_selected = sys - systems_stack;

      /* select the current system and make a path to it */
      if (!shifted) {
          if (map_path)
             free(map_path);
          map_path  = NULL;
          map_npath = 0;
      }

      /* Try to make path if is reachable. */
      if (space_sysReachable(sys)) {
         if (!shifted)
            map_path = map_getJumpPath( &map_npath,
                  cur_system->name, sys->name, 0, 1, NULL );
         else
            map_path = map_getJumpPath( &map_npath,
                  cur_system->name, sys->name, 0, 1, map_path );

         if (map_npath==0) {
            player_hyperspacePreempt(0);
            player_targetHyperspaceSet( -1 );
            player_autonavAbortJump(NULL);
            window_disableButton( wid, "btnAutonav" );
         }
         else  {
            /* see if it is a valid hyperspace target */
            for (i=0; i<cur_system->njumps; i++) {
               if (map_path[0] == cur_system->jumps[i].target) {
                  player_hyperspacePreempt(1);
                  player_targetHyperspaceSet( i );
                  break;
               }
            }
            window_enableButton( wid, "btnAutonav" );
         }
      }
      else { /* unreachable. */
         player_targetHyperspaceSet( -1 );
         player_autonavAbortJump(NULL);
         window_disableButton( wid, "btnAutonav" );
      }
   }

   map_update(wid);
   gui_setNav();
}

/*
 * A* algorithm for shortest path finding
 *
 * Note since that we can't actually get an admissible heurestic for A* this is
 * in reality just Djikstras. I've removed the heurestic bit to make sure I
 * don't try to implement an admissible heuristic when I'm pretty sure there is
 * none.
 */
/**
 * @brief Node structure for A* pathfinding.
 */
typedef struct SysNode_ {
   struct SysNode_ *next; /**< Next node */
   struct SysNode_ *gnext; /**< Next node in the garbage collector. */

   struct SysNode_ *parent; /**< Parent node. */
   StarSystem* sys; /**< System in node. */
   int g; /**< step */
} SysNode; /**< System Node for use in A* pathfinding. */
static SysNode *A_gc;
/* prototypes */
static SysNode* A_newNode( StarSystem* sys );
static int A_g( SysNode* n );
static SysNode* A_add( SysNode *first, SysNode *cur );
static SysNode* A_rm( SysNode *first, StarSystem *cur );
static SysNode* A_in( SysNode *first, StarSystem *cur );
static SysNode* A_lowest( SysNode *first );
static void A_freeList( SysNode *first );
static int map_decorator_parse( MapDecorator *temp, xmlNodePtr parent );
/** @brief Creates a new node link to star system. */
static SysNode* A_newNode( StarSystem* sys )
{
   SysNode* n;

   n        = malloc(sizeof(SysNode));

   n->next  = NULL;
   n->sys   = sys;

   n->gnext = A_gc;
   A_gc     = n;

   return n;
}
/** @brief Gets the g from a node. */
static int A_g( SysNode* n )
{
   return n->g;
}
/** @brief Adds a node to the linked list. */
static SysNode* A_add( SysNode *first, SysNode *cur )
{
   SysNode *n;

   if (first == NULL)
      return cur;

   n = first;
   while (n->next != NULL)
      n = n->next;
   n->next = cur;

   return first;
}
/* @brief Removes a node from a linked list. */
static SysNode* A_rm( SysNode *first, StarSystem *cur )
{
   SysNode *n, *p;

   if (first->sys == cur) {
      n = first->next;
      first->next = NULL;
      return n;
   }

   p = first;
   n = p->next;
   do {
      if (n->sys == cur) {
         n->next = NULL;
         p->next = n->next;
         break;
      }
      p = n;
   } while ((n=n->next) != NULL);

   return first;
}
/** @brief Checks to see if node is in linked list. */
static SysNode* A_in( SysNode *first, StarSystem *cur )
{
   SysNode *n;

   if (first == NULL)
      return NULL;

   n = first;
   do {
      if (n->sys == cur)
         return n;
   } while ((n=n->next) != NULL);
   return NULL;
}
/** @brief Returns the lowest ranking node from a linked list of nodes. */
static SysNode* A_lowest( SysNode *first )
{
   SysNode *lowest, *n;

   if (first == NULL)
      return NULL;

   n = first;
   lowest = n;
   do {
      if (n->g < lowest->g)
         lowest = n;
   } while ((n=n->next) != NULL);
   return lowest;
}
/** @brief Frees a linked list. */
static void A_freeList( SysNode *first )
{
   SysNode *p, *n;

   if (first == NULL)
      return;

   p = NULL;
   n = first;
   do {
      if (p != NULL)
         free(p);
      p = n;
   } while ((n=n->gnext) != NULL);
   free(p);
}

/** @brief Sets map_zoom to zoom and recreates the faction disk texture. */
void map_setZoom(double zoom)
{
   map_zoom = zoom;
}

/**
 * @brief Gets the jump path between two systems.
 *
 *    @param[out] njumps Number of jumps in the path.
 *    @param sysstart Name of the system to start from.
 *    @param sysend Name of the system to end at.
 *    @param ignore_known Whether or not to ignore if systems and jump points are known.
 *    @param show_hidden Whether or not to use hidden jumps points.
 *    @param the old star system (if we're merely extending the list)
 *    @return NULL on failure, the list of njumps elements systems in the path.
 */
StarSystem** map_getJumpPath( int* njumps, const char* sysstart,
    const char* sysend, int ignore_known, int show_hidden,
    StarSystem** old_data )
{
   int i, j, cost, ojumps;

   StarSystem *sys, *ssys, *esys, **res;
   JumpPoint *jp;

   SysNode *cur,   *neighbour;
   SysNode *open,  *closed;
   SysNode *ocost, *ccost;

   A_gc = NULL;

   /* initial and target systems */
   ssys = system_get(sysstart); /* start */
   esys = system_get(sysend); /* goal */

   /* Set up. */
   ojumps = 0;
   if ((old_data != NULL) && (*njumps>0)) {
      ssys   = system_get( old_data[ (*njumps)-1 ]->name );
      ojumps = *njumps;
   }

   /* Check self. */
   if ((ssys == esys) || (ssys->njumps==0)) {
      (*njumps) = 0;
      if (old_data != NULL)
         free( old_data );
      return NULL;
   }

   /* system target must be known and reachable */
   if (!ignore_known && !sys_isKnown(esys) && !space_sysReachable(esys)) {
      /* can't reach - don't make path */
      (*njumps) = 0;
      if (old_data != NULL)
         free( old_data );
      return NULL;
   }

   /* start the linked lists */
   open     = closed = NULL;
   cur      = A_newNode( ssys );
   cur->parent = NULL;
   cur->g   = 0;
   open     = A_add( open, cur ); /* Initial open node is the start system */

   j = 0;
   while ((cur = A_lowest(open))) {
      /* End condition. */
      if (cur->sys == esys)
         break;

      /* Break if infinite loop. */
      j++;
      if (j > MAP_LOOP_PROT)
         break;

      /* Get best from open and toss to closed */
      open   = A_rm( open, cur->sys );
      closed = A_add( closed, cur );
      cost   = A_g(cur) + 1; /* Base unit is jump and always increases by 1. */

      for (i=0; i<cur->sys->njumps; i++) {
         jp  = &cur->sys->jumps[i];
         sys = jp->target;

         /* Make sure it's reachable */
         if (!ignore_known) {
            if (!jp_isKnown(jp))
               continue;
            if (!sys_isKnown(sys) && !space_sysReachable(sys))
               continue;
         }
         if (jp_isFlag( jp, JP_EXITONLY ))
            continue;

         /* Skip hidden jumps if they're not specifically requested */
         if (!show_hidden && jp_isFlag( jp, JP_HIDDEN ))
            continue;

         /* Check to see if it's already in the closed set. */
         ccost = A_in(closed, sys);
         if ((ccost != NULL) && (cost >= A_g(ccost)))
            continue;
            //closed = A_rm( closed, sys );

         /* Remove if it exists and current is better. */
         ocost = A_in(open, sys);
         if (ocost != NULL) {
            if (cost < A_g(ocost))
               open = A_rm( open, sys ); /* New path is better */
            else
               continue; /* This node is worse, so ignore it. */
         }

         /* Create the node. */
         neighbour         = A_newNode( sys );
         neighbour->parent = cur;
         neighbour->g      = cost;
         open              = A_add( open, neighbour );
      }

      /* Safety check in case not linked. */
      if (open == NULL)
         break;
   }

   /* Build path backwards if not broken from loop. */
   if (esys == cur->sys) {
      (*njumps) = A_g(cur);
      if (old_data == NULL)
         res      = malloc( sizeof(StarSystem*) * (*njumps) );
      else {
         *njumps  = *njumps + ojumps;
         res      = realloc( old_data, sizeof(StarSystem*) * (*njumps) );
      }
      /* Build path. */
      for (i=0; i<((*njumps)-ojumps); i++) {
         res[(*njumps)-i-1] = cur->sys;
         cur                = cur->parent;
      }
   }
   else {
      (*njumps) = 0;
      res = NULL;
      if (old_data != NULL)
         free( old_data );
   }

   /* free the linked lists */
   A_freeList(A_gc);
   return res;
}


/**
 * @brief Marks maps around a radius of currently system as known.
 *
 *    @param targ_sys System at center of the "known" circle.
 *    @param r Radius (in jumps) to mark as known.
 *    @return 0 on success.
 */
int map_map( const Outfit *map )
{
   int i;

   for (i=0; i<array_size(map->u.map->systems);i++)
      sys_setFlag(map->u.map->systems[i], SYSTEM_KNOWN);

   for (i=0; i<array_size(map->u.map->assets);i++)
      planet_setKnown(map->u.map->assets[i]);

   for (i=0; i<array_size(map->u.map->jumps);i++)
      jp_setFlag(map->u.map->jumps[i], JP_KNOWN);

   return 1;
}


/**
 * @brief Check to see if map data is already mapped (known).
 *
 *    @param map Map outfit to check.
 *    @return 1 if already mapped, 0 if it wasn't.
 */
int map_isMapped( const Outfit* map )
{
   int i;

   for (i=0; i<array_size(map->u.map->systems);i++)
      if (!sys_isKnown(map->u.map->systems[i]))
         return 0;

   for (i=0; i<array_size(map->u.map->assets);i++)
      if (!planet_isKnown(map->u.map->assets[i]))
         return 0;

   for (i=0; i<array_size(map->u.map->jumps);i++)
      if (!jp_isKnown(map->u.map->jumps[i]))
         return 0;

   return 1;
}


/**
 * @brief Maps a local map.
 */
int localmap_map( const Outfit *lmap )
{
   int i;
   JumpPoint *jp;
   Planet *p;
   double detect, mod;

   if (cur_system==NULL)
      return 0;

   mod = pow2( 200. / (cur_system->interference + 200.) );

   detect = lmap->u.lmap.jump_detect;
   for (i=0; i<cur_system->njumps; i++) {
      jp = &cur_system->jumps[i];
      if (jp_isFlag(jp, JP_EXITONLY) || jp_isFlag(jp, JP_HIDDEN))
         continue;
      if (mod*jp->hide <= detect)
         jp_setFlag( jp, JP_KNOWN );
   }

   detect = lmap->u.lmap.asset_detect;
   for (i=0; i<cur_system->nplanets; i++) {
      p = cur_system->planets[i];
      if (p->real != ASSET_REAL)
         continue;
      if (mod*p->hide <= detect)
         planet_setKnown( p );
   }
   return 0;
}

/**
 * @brief Checks to see if the local map is mapped.
 */
int localmap_isMapped( const Outfit *lmap )
{
   int i;
   JumpPoint *jp;
   Planet *p;
   double detect, mod;

   if (cur_system==NULL)
      return 1;

   mod = pow2( 200. / (cur_system->interference + 200.) );

   detect = lmap->u.lmap.jump_detect;
   for (i=0; i<cur_system->njumps; i++) {
      jp = &cur_system->jumps[i];
      if (jp_isFlag(jp, JP_EXITONLY) || jp_isFlag(jp, JP_HIDDEN))
         continue;
      if ((mod*jp->hide <= detect) && !jp_isKnown( jp ))
         return 0;
   }

   detect = lmap->u.lmap.asset_detect;
   for (i=0; i<cur_system->nplanets; i++) {
      p = cur_system->planets[i];
      if (p->real != ASSET_REAL)
         continue;
      if ((mod*p->hide <= detect) && !planet_isKnown( p ))
         return 0;
   }
   return 1;
}


/**
 * @brief Shows a map at x, y (relative to wid) with size w,h.
 *
 *    @param wid Window to show map on.
 *    @param x X position to put map at.
 *    @param y Y position to put map at.
 *    @param w Width of map to open.
 *    @param h Height of map to open.
 *    @param zoom Default zoom to use.
 */
void map_show( int wid, int x, int y, int w, int h, double zoom )
{
   StarSystem *sys;

   /* mark systems as needed */
   mission_sysMark();

   /* Set position to focus on current system. */
   map_xpos = cur_system->pos.x * zoom;
   map_ypos = cur_system->pos.y * zoom;

   /* Set zoom. */
   map_setZoom(zoom);

   /* Make sure selected is valid. */
   sys = system_getIndex( map_selected );
   if (!(sys_isFlag(sys, SYSTEM_MARKED | SYSTEM_CMARKED)) &&
         !sys_isKnown(sys) && !space_sysReachable(sys))
      map_selectCur();

   window_addCust( wid, x, y, w, h,
         "cstMap", 1, map_render, map_mouse, NULL );
}


/**
 * @brief Centers the map on a planet.
 *
 *    @param sys System to center the map on.
 *    @return 0 on success.
 */
int map_center( const char *sys )
{
   StarSystem *ssys;

   /* Get the system. */
   ssys = system_get( sys );
   if (ssys == NULL)
      return -1;

   /* Center on the system. */
   map_xpos = ssys->pos.x * map_zoom;
   map_ypos = ssys->pos.y * map_zoom;

   return 0;
}

/**
 * @brief Loads all the map decorators.
 *
 *    @return 0 on success.
 */
int map_load (void)
{
   size_t bufsize;
   char *buf;
   xmlNodePtr node;
   xmlDocPtr doc;

   /* Load the file. */
   buf = ndata_read( MAP_DECORATOR_DATA_PATH, &bufsize);
   if (buf == NULL)
      return -1;

   /* Handle the XML. */
   doc = xmlParseMemory( buf, bufsize );
   if (doc == NULL) {
      WARN("'%s' is not valid XML.", MAP_DECORATOR_DATA_PATH);
      return -1;
   }

   node = doc->xmlChildrenNode; /* map node */
   if (strcmp((char*)node->name,"map")) {
      ERR("Malformed "MAP_DECORATOR_DATA_PATH" file: missing root element 'map'");
      return -1;
   }

   node = node->xmlChildrenNode;
   if (node == NULL) {
      ERR("Malformed "MAP_DECORATOR_DATA_PATH" file: does not contain elements");
      return -1;
   }

   do {
      xml_onlyNodes(node);
      if (xml_isNode(node, "decorator")) {

         /* Make room for decorators. */
         decorator_stack = realloc(decorator_stack,
               sizeof(MapDecorator)*(++decorator_nstack));

         /* Load decorator. */
         map_decorator_parse(&decorator_stack[decorator_nstack-1], node);

      }
      else
         WARN("'"MAP_DECORATOR_DATA_PATH"' has unknown node '%s'.", node->name);
   } while (xml_nextNode(node));

   xmlFreeDoc(doc);
   free(buf);

   DEBUG("Loaded %d map decorators.", decorator_nstack);

   return 0;
}

static int map_decorator_parse( MapDecorator *temp, xmlNodePtr parent ) {
   xmlNodePtr node;

   /* Clear memory. */
   memset( temp, 0, sizeof(MapDecorator) );

   temp->detection_radius=10;
   temp->auto_fade=0;

   /* Parse body. */
   node = parent->xmlChildrenNode;
   do {
      xml_onlyNodes(node);
      xmlr_float(node, "x", temp->x);
      xmlr_float(node, "y", temp->y);
      xmlr_int(node, "auto_fade", temp->auto_fade);
      xmlr_int(node, "detection_radius", temp->detection_radius);
      if (xml_isNode(node,"image")) {
         temp->image = xml_parseTexture( node,
               MAP_DECORATOR_GFX_PATH"%s.png", 1, 1, OPENGL_TEX_MIPMAPS );

         if (temp->image == NULL) {
            WARN("Could not load map decorator texture '%s'.", xml_get(node));
         }

         continue;
      }
      WARN("Map decorator has unknown node '%s'.", node->name);
   } while (xml_nextNode(node));

   return 0;
}
