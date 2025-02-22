/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file ship.c
 *
 * @brief Handles the ship details.
 */


#include "ship.h"

#include "naev.h"

#include "nstring.h"
#include <limits.h>

#include "nxml.h"

#include "log.h"
#include "ndata.h"
#include "toolkit.h"
#include "array.h"
#include "conf.h"
#include "npng.h"
#include "colour.h"
#include "shipstats.h"
#include "slots.h"
#include "nfile.h"
#include "unistd.h"


#define XML_SHIP  "ship" /**< XML individual ship identifier. */

#define SHIP_EXT     ".png" /**< Ship graphics extension format. */
#define SHIP_ENGINE  "_engine" /**< Engine graphic extension. */
#define SHIP_TARGET  "_target" /**< Target graphic extension. */
#define SHIP_COMM    "_comm" /**< Communication graphic extension. */

#define VIEW_WIDTH   300 /**< Ship view window width. */
#define VIEW_HEIGHT  300 /**< Ship view window height. */

#define BUTTON_WIDTH  80 /**< Button width in ship view window. */
#define BUTTON_HEIGHT 30 /**< Button height in ship view window. */

#define STATS_DESC_MAX 256 /**< Maximum length for statistics description. */


static Ship* ship_stack = NULL; /**< Stack of ships available in the game. */


/*
 * Prototypes
 */
static int ship_loadGFX( Ship *temp, char *buf, int sx, int sy, int engine );
static int ship_loadPLG( Ship *temp, char *buf );
static int ship_parse( Ship *temp, xmlNodePtr parent );


/**
 * @brief Gets a ship based on its name.
 *
 *    @param name Name to match.
 *    @return Ship matching name or NULL if not found.
 */
Ship* ship_get( const char* name )
{
   Ship *temp;
   int i;

   temp = ship_stack;
   for (i=0; i < array_size(ship_stack); i++)
      if (strcmp(temp[i].name, name)==0)
         return &temp[i];

   WARN(_("Ship %s does not exist"), name);
   return NULL;
}


/**
 * @brief Gets a ship based on its name without warning.
 *
 *    @param name Name to match.
 *    @return Ship matching name or NULL if not found.
 */
Ship* ship_getW( const char* name )
{
   Ship *temp;
   int i;

   temp = ship_stack;
   for (i=0; i < array_size(ship_stack); i++)
      if (strcmp(temp[i].name, name)==0)
         return &temp[i];

   return NULL;
}


/**
 * @brief Checks to see if an ship exists matching name (case insensitive).
 */
const char *ship_existsCase( const char* name )
{
   int i;
   for (i=0; i<array_size(ship_stack); i++)
      if (strcasecmp(name,ship_stack[i].name)==0)
         return ship_stack[i].name;
   return NULL;
}




/**
 * @brief Gets all the ships.
 */
Ship* ship_getAll( int *n )
{
   *n = array_size(ship_stack);
   return ship_stack;
}


/**
 * @brief Comparison function for qsort().
 */
int ship_compareTech( const void *arg1, const void *arg2 )
{
   const Ship *s1, *s2;

   /* Get ships. */
   s1 = * (const Ship**) arg1;
   s2 = * (const Ship**) arg2;

   /* Compare class. */
   if (s1->class < s2->class)
      return +1;
   else if (s1->class > s2->class)
      return -1;

   /* Compare price. */
   if (s1->price < s2->price)
      return +1;
   else if (s1->price > s2->price)
      return -1;

   /* Same. */
   return strcmp( s1->name, s2->name );
}


/**
 * @brief Gets the ship's class name in human readable form.
 *
 *    @param s Ship to get the class name from.
 *    @return The human readable class name.
 */
char* ship_class( Ship* s )
{
   switch (s->class) {
      case SHIP_CLASS_NULL:
         return "NULL";

      /* Civilian. */
      case SHIP_CLASS_YACHT:
         return gettext_noop("Yacht");
      case SHIP_CLASS_LUXURY_YACHT:
         return gettext_noop("Luxury Yacht");
      case SHIP_CLASS_CRUISE_SHIP:
         return gettext_noop("Cruise Ship");

      /* Merchant. */
      case SHIP_CLASS_COURIER:
         return gettext_noop("Courier");
      case SHIP_CLASS_ARMOURED_TRANSPORT:
         return gettext_noop("Armoured Transport");
      case SHIP_CLASS_FREIGHTER:
         return gettext_noop("Freighter");
      case SHIP_CLASS_BULK_CARRIER:
         return gettext_noop("Bulk Carrier");

      /* Military. */
      case SHIP_CLASS_SCOUT:
         return gettext_noop("Scout");
      case SHIP_CLASS_FIGHTER:
         return gettext_noop("Fighter");
      case SHIP_CLASS_BOMBER:
         return gettext_noop("Bomber");
      case SHIP_CLASS_CORVETTE:
         return gettext_noop("Corvette");
      case SHIP_CLASS_DESTROYER:
         return gettext_noop("Destroyer");
      case SHIP_CLASS_CRUISER:
         return gettext_noop("Cruiser");
      case SHIP_CLASS_CARRIER:
         return gettext_noop("Carrier");

      /* Robotic. */
      case SHIP_CLASS_DRONE:
         return gettext_noop("Drone");
      case SHIP_CLASS_HEAVY_DRONE:
         return gettext_noop("Heavy Drone");
      case SHIP_CLASS_MOTHERSHIP:
         return gettext_noop("Mothership");

      /* Unknown. */
      default:
         return gettext_noop("Unknown");
   }
}


/**
 * @brief Gets the machine ship class identifier from a human readable string.
 *
 *    @param str String to extract ship class identifier from.
 */
ShipClass ship_classFromString( char* str )
{
   /* Civilian */
   if (strcmp(str,"Yacht")==0)
      return SHIP_CLASS_YACHT;
   else if (strcmp(str,"Luxury Yacht")==0)
      return SHIP_CLASS_LUXURY_YACHT;
   else if (strcmp(str,"Cruise Ship")==0)
      return SHIP_CLASS_CRUISE_SHIP;

   /* Merchant. */
   else if (strcmp(str,"Courier")==0)
      return SHIP_CLASS_COURIER;
   else if (strcmp(str,"Freighter")==0)
      return SHIP_CLASS_FREIGHTER;
   else if (strcmp(str,"Armoured Transport")==0)
      return SHIP_CLASS_ARMOURED_TRANSPORT;
   else if (strcmp(str,"Bulk Carrier")==0)
      return SHIP_CLASS_BULK_CARRIER;

   /* Military */
   else if (strcmp(str,"Scout")==0)
      return SHIP_CLASS_SCOUT;
   else if (strcmp(str,"Fighter")==0)
      return SHIP_CLASS_FIGHTER;
   else if (strcmp(str,"Bomber")==0)
      return SHIP_CLASS_BOMBER;
   else if (strcmp(str,"Corvette")==0)
      return SHIP_CLASS_CORVETTE;
   else if (strcmp(str,"Destroyer")==0)
      return SHIP_CLASS_DESTROYER;
   else if (strcmp(str,"Cruiser")==0)
      return SHIP_CLASS_CRUISER;
   else if (strcmp(str,"Carrier")==0)
      return SHIP_CLASS_CARRIER;

   /* Robotic */
   else if (strcmp(str,"Drone")==0)
      return SHIP_CLASS_DRONE;
   else if (strcmp(str,"Heavy Drone")==0)
      return SHIP_CLASS_HEAVY_DRONE;
   else if (strcmp(str,"Mothership")==0)
      return SHIP_CLASS_MOTHERSHIP;

  /* Unknown */
  return SHIP_CLASS_NULL;
}


/**
 * @brief Gets the ship's base price (no outfits).
 */
credits_t ship_basePrice( const Ship* s )
{
   credits_t price;

   /* Get ship base price. */
   price = s->price;

   if (price < 0) {
      WARN(_("Negative ship base price!"));
      price = 0;
   }

   return price;
}


/**
 * @brief The ship buy price, includes default outfits.
 */
credits_t ship_buyPrice( const Ship* s )
{
   int i;
   credits_t price;
   Outfit *o;

   /* Get base price. */
   price = ship_basePrice(s);

   for (i=0; i<s->outfit_nstructure; i++) {
      o = s->outfit_structure[i].data;
      if (o != NULL)
         price += o->price;
   }
   for (i=0; i<s->outfit_nutility; i++) {
      o = s->outfit_utility[i].data;
      if (o != NULL)
         price += o->price;
   }
   for (i=0; i<s->outfit_nweapon; i++) {
      o = s->outfit_weapon[i].data;
      if (o != NULL)
         price += o->price;
   }

   return price;
}


/**
 * @brief Loads the ship's comm graphic.
 *
 * Must be freed afterwards.
 */
glTexture* ship_loadCommGFX( Ship* s )
{
   if (s->gfx_comm != NULL)
      return gl_newImage( s->gfx_comm, 0 );
   return NULL;
}


/**
 * @brief Generates a target graphic for a ship.
 */
static int ship_genTargetGFX( Ship *temp, SDL_Surface *surface, int sx, int sy )
{
   SDL_Surface *gfx, *gfx_store;
   int potw, poth, potw_store, poth_store;
   int x, y, sw, sh;
   SDL_Rect rtemp, dstrect;
#if 0 /* Required for scanlines. */
   int i, j;
   uint32_t *pix;
   double r, g, b, a;
   double h, s, v;
#endif
   char buf[PATH_MAX];

   /* Get sprite size. */
   sw = temp->gfx_space->w / sx;
   sh = temp->gfx_space->h / sy;

   /* POT size. */
   if (gl_needPOT()) {
      potw = gl_pot( sw );
      poth = gl_pot( sh );
      potw_store = gl_pot( SHIP_TARGET_W );
      poth_store = gl_pot( SHIP_TARGET_H );
   }
   else {
      potw = sw;
      poth = sh;
      potw_store = SHIP_TARGET_W;
      poth_store = SHIP_TARGET_H;
   }

   /* Create the surface. */
   SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);

   /* create the temp POT surface */
   gfx = SDL_CreateRGBSurface( 0, potw, poth,
         surface->format->BytesPerPixel*8, RGBAMASK );
   gfx_store = SDL_CreateRGBSurface( 0, potw_store, poth_store,
         surface->format->BytesPerPixel*8, RGBAMASK );

   if (gfx == NULL) {
      WARN( _("Unable to create ship '%s' targeting surface."), temp->name );
      return -1;
   }

   /* Copy over for target. */
   gl_getSpriteFromDir( &x, &y, temp->gfx_space, M_PI* 5./4. );
   rtemp.x = sw * x;
   rtemp.y = sh * (temp->gfx_space->sy-y-1);
   rtemp.w = sw;
   rtemp.h = sh;
   dstrect.x = 0;
   dstrect.y = 0;
   dstrect.w = rtemp.w;
   dstrect.h = rtemp.h;
   SDL_BlitSurface( surface, &rtemp, gfx, &dstrect );

   /* Copy over for store. */
   dstrect.x = (SHIP_TARGET_W - sw) / 2;
   dstrect.y = (SHIP_TARGET_H - sh) / 2;
   dstrect.w = rtemp.w;
   dstrect.h = rtemp.h;
   SDL_BlitSurface( surface, &rtemp, gfx_store, &dstrect );

   /* Load the store surface. */
   nsnprintf( buf, sizeof(buf), "%s_gfx_store.png", temp->name );
   temp->gfx_store = gl_loadImagePad( buf, gfx_store, 0, SHIP_TARGET_W, SHIP_TARGET_H, 1, 1, 1 );

#if 0 /* Disabled for now due to issues with larger sprites. */
   /* Some filtering. */
   for (j=0; j<sh; j++) {
      for (i=0; i<sw; i++) {
         pix   = (uint32_t*) ((uint8_t*) gfx->pixels + j*gfx->pitch + i*gfx->format->BytesPerPixel);
         r     = ((double) (*pix & RMASK)) / (double) RMASK;
         g     = ((double) (*pix & GMASK)) / (double) GMASK;
         b     = ((double) (*pix & BMASK)) / (double) BMASK;
         a     = ((double) (*pix & AMASK)) / (double) AMASK;
         if (j%2) /* Add scanlines. */
            a *= 0.5;

         /* Convert to HSV. */
         col_rgb2hsv( &h, &s, &v, r, g, b );

         h  = 0.0;
         s  = 1.0;
         v *= 1.5;

         /* Convert back to RGB. */
         col_hsv2rgb( &r, &g, &b, h, s, v );

         /* Convert to pixel. */
         *pix =   ((uint32_t) (r*RMASK) & RMASK) |
                  ((uint32_t) (g*GMASK) & GMASK) |
                  ((uint32_t) (b*BMASK) & BMASK) |
                  ((uint32_t) (a*AMASK) & AMASK);
      }
   }
#endif

   /* Load the surface. */
   nsnprintf( buf, sizeof(buf), "%s_gfx_target.png", temp->name );
   temp->gfx_target = gl_loadImagePad( buf, gfx, 0, sw, sh, 1, 1, 1 );

   return 0;
}


/**
 * @brief Loads the space graphics for a ship from an image.
 *
 *    @param temp Ship to load into.
 *    @param str Path of the image to use.
 */
static int ship_loadSpaceImage( Ship *temp, char *str, int sx, int sy )
{
   png_uint_32 w, h;
   SDL_RWops *rw;
   npng_t *npng;
   SDL_Surface *surface;
   int ret;

   /* Load the space sprite. */
   rw    = ndata_rwops( str );
   npng  = npng_open( rw );
   npng_dim( npng, &w, &h );
   surface = npng_readSurface( npng, gl_needPOT(), 1 );

   /* Load the texture. */
   temp->gfx_space = gl_loadImagePadTrans( str, surface, rw,
         OPENGL_TEX_MAPTRANS | OPENGL_TEX_MIPMAPS,
         w, h, sx, sy, 0 );

   /* Create the target graphic. */
   ret = ship_genTargetGFX( temp, surface, sx, sy );
   if (ret != 0)
      return ret;

   /* Free stuff. */
   npng_close( npng );
   SDL_RWclose( rw );
   SDL_FreeSurface( surface );

   /* Calculate mount angle. */
   temp->mangle  = 2.*M_PI;
   temp->mangle /= temp->gfx_space->sx * temp->gfx_space->sy;
   return 0;
}


/**
 * @brief Loads the space graphics for a ship from an image.
 *
 *    @param temp Ship to load into.
 *    @param str Path of the image to use.
 */
static int ship_loadEngineImage( Ship *temp, char *str, int sx, int sy )
{
   temp->gfx_engine = gl_newSprite( str, sx, sy, OPENGL_TEX_MIPMAPS );
   return (temp->gfx_engine != NULL);
}


/**
 * @brief Loads the graphics for a ship.
 *
 *    @param temp Ship to load into.
 *    @param buf Name of the texture to work with.
 */
static int ship_loadGFX( Ship *temp, char *buf, int sx, int sy, int engine )
{
   char base[PATH_MAX], str[PATH_MAX];
   int i;

   /* Get base path. */
   for (i=0; i<PATH_MAX; i++) {
      if ((buf[i] == '\0') || (buf[i] == '_')) {
         base[i] = '\0';
         break;
      }
      base[i] = buf[i];
   }
   if (i>=PATH_MAX) {
      WARN(_("Failed to get base path of '%s'."), buf);
      return -1;
   }

   nsnprintf( str, PATH_MAX, SHIP_GFX_PATH"%s/%s"SHIP_EXT, base, buf );
   ship_loadSpaceImage( temp, str, sx, sy );

   /* Load the engine sprite .*/
   if (engine && conf.engineglow && conf.interpolate) {
      nsnprintf( str, PATH_MAX, SHIP_GFX_PATH"%s/%s"SHIP_ENGINE SHIP_EXT, base, buf );
      ship_loadEngineImage( temp, str, sx, sy );
      if (temp->gfx_engine == NULL)
         WARN(_("Ship '%s' does not have an engine sprite (%s)."), temp->name, str );
   }

   /* Get the comm graphic for future loading. */
   nsnprintf( str, PATH_MAX, SHIP_GFX_PATH"%s/%s"SHIP_COMM SHIP_EXT, base, buf );
   temp->gfx_comm = strdup(str);

   return 0;
}


/**
 * @brief Loads the collision polygon for a ship.
 *
 *    @param temp Ship to load into.
 *    @param buf Name of the file.
 */
static int ship_loadPLG( Ship *temp, char *buf )
{
   size_t bufsize;
   char *file;
   int sl;
   CollPoly *polygon;
   xmlDocPtr doc;
   xmlNodePtr node, cur;

   temp->npolygon = 0;

   sl   = strlen(buf)+strlen(SHIP_POLYGON_PATH)+strlen(".xml")+1;
   file = malloc( sl );

   nsnprintf( file, sl, "%s%s.xml", SHIP_POLYGON_PATH, buf );

   /* See if the file does exist. */
   if (!ndata_exists(file)) {
      WARN(_("%s xml collision polygon does not exist!\n \
               Please use the script 'polygon_from_sprite.py' if sprites are used,\n \
               And 'polygonSTL.py' if 3D model is used in game.\n \
               These files can be found in naev's artwork repo."), file);
      free(file);
      return 0;
   }

   /* Load the XML. */
   buf  = ndata_read( file, &bufsize );
   doc  = xmlParseMemory( buf, bufsize );
   free(buf);

   if (doc == NULL) {
      WARN(_("%s file is invalid xml!"), file);
      free(file);
      return 0;
   }

   node = doc->xmlChildrenNode; /* First polygon node */
   if (node == NULL) {
      xmlFreeDoc(doc);
      WARN(_("Malformed %s file: does not contain elements"), file);
      free(file);
      return 0;
   }

   free(file);

   do { /* load the polygon data */
      if (xml_isNode(node,"polygons")) {
         cur = node->children;
         temp->polygon = malloc( sizeof(CollPoly) );
         do {
            if (xml_isNode(cur,"polygon")) {
               temp->npolygon++;
               temp->polygon = realloc( temp->polygon, sizeof(CollPoly) * temp->npolygon );
               polygon = &temp->polygon[temp->npolygon-1];

               LoadPolygon( polygon, cur );
            }
         } while (xml_nextNode(cur));
      }
   } while (xml_nextNode(node));

   xmlFreeDoc(doc);
   return 0;
}


/**
 * @brief Parses a slot for a ship.
 *
 *    @param temp Ship to be parsed.
 *    @param slot Slot being parsed.
 *    @param type Type of the slot.
 *    @param node Node containing the data.
 *    @return 0 on success.
 */
static int ship_parseSlot( Ship *temp, ShipOutfitSlot *slot, OutfitSlotType type, xmlNodePtr node )
{
   OutfitSlotSize base_size;
   char *buf, *typ;
   Outfit *o;

   /* Parse size. */
   xmlr_attr( node, "size", buf );
   if (buf != NULL)
      base_size = outfit_toSlotSize( buf );
   else {
      if ((temp->class == SHIP_CLASS_BULK_CARRIER) ||
            (temp->class == SHIP_CLASS_CRUISER) ||
            (temp->class == SHIP_CLASS_CARRIER) ||
            (temp->class == SHIP_CLASS_MOTHERSHIP)) {
         typ       = "Large";
         base_size = OUTFIT_SLOT_SIZE_HEAVY;
      }
      else if ((temp->class == SHIP_CLASS_CRUISE_SHIP) ||
            (temp->class == SHIP_CLASS_FREIGHTER) ||
            (temp->class == SHIP_CLASS_DESTROYER) ||
            (temp->class == SHIP_CLASS_CORVETTE) ||
            (temp->class == SHIP_CLASS_ARMOURED_TRANSPORT)) {
         typ       = "Medium";
         base_size = OUTFIT_SLOT_SIZE_MEDIUM;
      }
      else {
         typ       = "Small";
         base_size = OUTFIT_SLOT_SIZE_LIGHT;
      }
      WARN(_("Ship '%s' has implicit slot size, setting to '%s'"),temp->name, typ);
   }
   free(buf);

   /* Get mount point for weapons. */
   if (type == OUTFIT_SLOT_WEAPON) {
      xmlr_attr(node,"x",buf);
      if (buf!=NULL) {
         slot->mount.x = atof(buf);
         free(buf);
      }
      else
         WARN(_("Ship '%s' missing 'x' element of 'weapon' slot."),temp->name);
      xmlr_attr(node,"y",buf);
      if (buf!=NULL) {
         slot->mount.y = atof(buf);
         /* Since we measure in pixels, we have to modify it so it
          *  doesn't get corrected by the ortho correction. */
         slot->mount.y *= M_SQRT2;
         free(buf);
      }
      else
         WARN(_("Ship '%s' missing 'y' element of 'weapon' slot."),temp->name);
      xmlr_attr(node,"h",buf);
      if (buf!=NULL) {
         slot->mount.h = atof(buf);
         free(buf);
      }
      else
         WARN(_("Ship '%s' missing 'h' element of 'weapon' slot."),temp->name);
   }

   /* Parse property. */
   xmlr_attr( node, "prop", buf );
   if (buf != NULL) {
      slot->slot.spid = sp_get( buf );
      slot->exclusive = sp_exclusive( slot->slot.spid );
      slot->required  = sp_required( slot->slot.spid );
      free( buf );
   }
   //TODO: consider inserting those two parse blocks below inside the parse block above

   /* Parse exclusive flag. */
   xmlr_attr( node, "exclusive", buf );
   if (buf != NULL) {
      slot->exclusive = atoi( buf );
      free( buf );
   }
   //TODO: decide if exclusive should even belong in ShipOutfitSlot, remove this hack, and fix slot->exclusive to slot->slot.exclusive in it's two previous occurrences, meaning three lines above and 12 lines above
   /* hack */
   slot->slot.exclusive=slot->exclusive;

   /* Parse required flag. */
   xmlr_attr( node, "required", buf );
   if (buf != NULL) {
      slot->required  = atoi( buf );
      free( buf );
   }

   /* Parse default outfit. */
   buf = xml_get(node);
   if (buf != NULL) {
      o = outfit_get( buf );
      if (o == NULL)
         WARN( _("Ship '%s' has default outfit '%s' which does not exist."), temp->name, buf );
      slot->data = o;
   }

   /* Set stuff. */
   slot->slot.size = base_size;
   slot->slot.type = type;

   /* Required slots need a default outfit. */
   if (slot->required && (slot->data == NULL))
      WARN(_("Ship '%s' has required slot without a default outfit."), temp->name);

   return 0;
}

/**
 * @brief Extracts the ingame ship from an XML node.
 *
 *    @param temp Ship to load data into.
 *    @param parent Node to get ship from.
 *    @return 0 on success.
 */
static int ship_parse( Ship *temp, xmlNodePtr parent )
{
   int i;
   xmlNodePtr cur, node;
   int sx, sy;
   char *stmp, *buf;
   char str[PATH_MAX];
   int l, m, h, engine;
   ShipStatList *ll;

   /* Clear memory. */
   memset( temp, 0, sizeof(Ship) );

   /* Defaults. */
   ss_statsInit( &temp->stats_array );

   /* Get name. */
   xmlr_attr(parent,"name",temp->name);
   if (temp->name == NULL)
      WARN( _("Ship in %s has invalid or no name"), SHIP_DATA_PATH );

   /* Datat that must be loaded first. */
   node = parent->xmlChildrenNode;
   do { /* load all the data */
      xml_onlyNodes(node);
      if (xml_isNode(node,"class")) {
         temp->class = ship_classFromString( xml_get(node) );
         continue;
      }
   } while (xml_nextNode(node));

   /* Load the rest of the data. */
   node = parent->xmlChildrenNode;
   do { /* load all the data */

      /* Only handle nodes. */
      xml_onlyNodes(node);

      if (xml_isNode(node,"GFX")) {

         /* Get base graphic name. */
         buf = xml_get(node);
         if (buf==NULL) {
            WARN(_("Ship '%s': GFX element is NULL"), temp->name);
            continue;
         }

         /* Get sprite size. */
         xmlr_attr(node, "sx", stmp );
         if (stmp != NULL) {
            sx = atoi(stmp);
            free(stmp);
         }
         else
            sx = 8;
         xmlr_attr(node, "sy", stmp );
         if (stmp != NULL) {
            sy = atoi(stmp);
            free(stmp);
         }
         else
            sy = 8;

         xmlr_attr(node, "noengine", stmp );
         if (stmp != NULL) {
            engine = 0;
            free(stmp);
         }
         else
            engine = 1;

         /* Load the graphics. */
         ship_loadGFX( temp, buf, sx, sy, engine );

         /* Load the polygon. */
         ship_loadPLG( temp, buf );

         /* Validity check: there must be 1 polygon per sprite. */
         if (temp->npolygon != sx*sy) {
            WARN(_("Ship '%s': the number of collision polygons is wrong.\n \
                    npolygon = %i and sx*sy = %i"),
                    temp->name, temp->npolygon, sx*sy);
         }

         continue;
      }

      if (xml_isNode(node,"gfx_space")) {

         /* Get path */
         buf = xml_get(node);
         if (buf==NULL) {
            WARN(_("Ship '%s': gfx_space element is NULL"), temp->name);
            continue;
         }
         nsnprintf( str, PATH_MAX, GFX_PATH"%s", buf );

         /* Get sprite size. */
         xmlr_attr(node, "sx", stmp );
         if (stmp != NULL) {
            sx = atoi(stmp);
            free(stmp);
         }
         else
            sx = 8;
         xmlr_attr(node, "sy", stmp );
         if (stmp != NULL) {
            sy = atoi(stmp);
            free(stmp);
         }
         else
            sy = 8;

         /* Load the graphics. */
         ship_loadSpaceImage( temp, str, sx, sy );

         continue;
      }

      if (xml_isNode(node,"gfx_engine")) {

         /* Get path */
         buf = xml_get(node);
         if (buf==NULL) {
            WARN(_("Ship '%s': gfx_engine element is NULL"), temp->name);
            continue;
         }
         nsnprintf( str, PATH_MAX, GFX_PATH"%s", buf );

         /* Get sprite size. */
         xmlr_attr(node, "sx", stmp );
         if (stmp != NULL) {
            sx = atoi(stmp);
            free(stmp);
         }
         else
            sx = 8;
         xmlr_attr(node, "sy", stmp );
         if (stmp != NULL) {
            sy = atoi(stmp);
            free(stmp);
         }
         else
            sy = 8;

         /* Load the graphics. */
         ship_loadEngineImage( temp, str, sx, sy );

         continue;
      }

      if (xml_isNode(node,"gfx_comm")) {

         /* Get path */
         buf = xml_get(node);
         if (buf==NULL) {
            WARN(_("Ship '%s': gfx_comm element is NULL"), temp->name);
            continue;
         }
         nsnprintf( str, PATH_MAX, GFX_PATH"%s", buf );
         temp->gfx_comm = strdup(str);
         continue;
      }
      if (xml_isNode(node,"gfx_overlays")) {
         cur = node->children;
         m = 2;
         temp->gfx_overlays = malloc( m*sizeof(glTexture*) );
         do {
            xml_onlyNodes(cur);
            if (xml_isNode(cur,"gfx_overlay")) {
               temp->gfx_noverlays += 1;
               if (temp->gfx_noverlays > m) {
                  m *= 2;
                  temp->gfx_overlays = realloc( temp->gfx_overlays, m*sizeof(glTexture) );
               }
               temp->gfx_overlays[ temp->gfx_noverlays-1 ] = xml_parseTexture( cur,
                     OVERLAY_GFX_PATH"%s.png", 1, 1, OPENGL_TEX_MIPMAPS );
            }
         } while (xml_nextNode(cur));
         continue;
      }

      xmlr_strd(node,"GUI",temp->gui);
      if (xml_isNode(node,"sound")) {
         temp->sound = sound_get( xml_get(node) );
         continue;
      }
      xmlr_strd(node,"base_type",temp->base_type);
      if (xml_isNode(node,"class")) {
         /* Already preemptively loaded, avoids warning. */
         continue;
      }
      xmlr_float(node,"time_mod",temp->dt_default);
      xmlr_long(node,"price",temp->price);
      xmlr_strd(node,"license",temp->license);
      xmlr_strd(node,"fabricator",temp->fabricator);
      xmlr_strd(node,"description",temp->description);
      xmlr_int(node,"rarity",temp->rarity);
      if (xml_isNode(node,"movement")) {
         cur = node->children;
         do {
            xml_onlyNodes(cur);
            xmlr_float(cur,"thrust",temp->thrust);
            xmlr_float(cur,"turn",temp->turn);
            xmlr_float(cur,"speed",temp->speed);
            /* All the xmlr_ stuff have continue cases. */
            WARN(_("Ship '%s' has unknown movement node '%s'."), temp->name, cur->name);
         } while (xml_nextNode(cur));
         continue;
      }
      if (xml_isNode(node,"health")) {
         cur = node->children;
         do {
            xml_onlyNodes(cur);
            xmlr_float(cur,"absorb",temp->dmg_absorb);
            xmlr_float(cur,"armour",temp->armour);
            xmlr_float(cur,"armour_regen",temp->armour_regen);
            xmlr_float(cur,"shield",temp->shield);
            xmlr_float(cur,"shield_regen",temp->shield_regen);
            xmlr_float(cur,"energy",temp->energy);
            xmlr_float(cur,"energy_regen",temp->energy_regen);
            /* All the xmlr_ stuff have continue cases. */
            WARN(_("Ship '%s' has unknown health node '%s'."), temp->name, cur->name);
         } while (xml_nextNode(cur));
         continue;
      }
      if (xml_isNode(node,"characteristics")) {
         cur = node->children;
         do {
            xml_onlyNodes(cur);
            xmlr_int(cur,"crew",temp->crew);
            xmlr_float(cur,"mass",temp->mass);
            xmlr_float(cur,"cpu",temp->cpu);
            xmlr_int(cur,"fuel",temp->fuel);
            xmlr_int(cur,"fuel_consumption",temp->fuel_consumption);
            xmlr_float(cur,"cargo",temp->cap_cargo);
            /* All the xmlr_ stuff have continue cases. */
            WARN(_("Ship '%s' has unknown characteristic node '%s'."), temp->name, cur->name);
         } while (xml_nextNode(cur));
         continue;
      }
      if (xml_isNode(node,"slots")) {
         /* First pass, get number of mounts. */
         cur = node->children;
         do {
            xml_onlyNodes(cur);
            if (xml_isNode(cur,"structure"))
               temp->outfit_nstructure++;
            else if (xml_isNode(cur,"utility"))
               temp->outfit_nutility++;
            else if (xml_isNode(cur,"weapon"))
               temp->outfit_nweapon++;
            else
               WARN(_("Ship '%s' has unknown slot node '%s'."), temp->name, cur->name);
         } while (xml_nextNode(cur));

         /* Allocate the space. */
         temp->outfit_structure  = calloc( temp->outfit_nstructure, sizeof(ShipOutfitSlot) );
         temp->outfit_utility    = calloc( temp->outfit_nutility, sizeof(ShipOutfitSlot) );
         temp->outfit_weapon     = calloc( temp->outfit_nweapon, sizeof(ShipOutfitSlot) );
         /* Second pass, initialize the mounts. */
         l = m = h = 0;
         cur = node->children;
         do {
            xml_onlyNodes(cur);
            if (xml_isNode(cur,"structure")) {
               ship_parseSlot( temp, &temp->outfit_structure[l], OUTFIT_SLOT_STRUCTURE, cur );
               l++;
            }
            if (xml_isNode(cur,"utility")) {
               ship_parseSlot( temp, &temp->outfit_utility[m], OUTFIT_SLOT_UTILITY, cur );
               m++;
            }
            if (xml_isNode(cur,"weapon")) {
               ship_parseSlot( temp, &temp->outfit_weapon[h], OUTFIT_SLOT_WEAPON, cur );
               h++;
            }
         } while (xml_nextNode(cur));
         continue;
      }

      /* Parse ship stats. */
      if (xml_isNode(node,"stats")) {
         cur = node->children;
         do {
            xml_onlyNodes(cur);
            ll = ss_listFromXML( cur );
            if (ll != NULL) {
               ll->next    = temp->stats;
               temp->stats = ll;
               continue;
            }
            WARN(_("Ship '%s' has unknown stat '%s'."), temp->name, cur->name);
         } while (xml_nextNode(cur));

         /* Load array. */
         ss_statsInit( &temp->stats_array );
         ss_statsModFromList( &temp->stats_array, temp->stats, NULL );

         /* Create description. */
         if (temp->stats != NULL) {
            temp->desc_stats = malloc( STATS_DESC_MAX );
            i = ss_statsListDesc( temp->stats, temp->desc_stats, STATS_DESC_MAX, 0 );
            if (i <= 0) {
               free( temp->desc_stats );
               temp->desc_stats = NULL;
            }
         }

         continue;
      }

      /* Used by in-valid and NSH utils, no in-game meaning. */
      if (xml_isNode(node,"mission"))
         continue;

      DEBUG(_("Ship '%s' has unknown node '%s'."), temp->name, node->name);
   } while (xml_nextNode(node));

   /* Post processing. */
   temp->dmg_absorb   /= 100.;
   temp->turn         *= M_PI / 180.; /* Convert to rad. */

   /* ship validator */
#define MELEMENT(o,s)      if (o) WARN( _("Ship '%s' missing '%s' element"), temp->name, s)
   MELEMENT(temp->name==NULL,"name");
   MELEMENT(temp->base_type==NULL,"base_type");
   MELEMENT((temp->gfx_space==NULL) || (temp->gfx_comm==NULL),"GFX");
   MELEMENT(temp->gui==NULL,"GUI");
   MELEMENT(temp->class==SHIP_CLASS_NULL,"class");
   MELEMENT(temp->price==0,"price");
   MELEMENT(temp->dt_default==0.,"time_mod");
   MELEMENT(temp->fabricator==NULL,"fabricator");
   MELEMENT(temp->description==NULL,"description");
   MELEMENT(temp->armour==0.,"armour");
   /*MELEMENT(temp->thrust==0.,"thrust");
   MELEMENT(temp->turn==0.,"turn");
   MELEMENT(temp->speed==0.,"speed");
   MELEMENT(temp->shield==0.,"shield");
   MELEMENT(temp->shield_regen==0.,"shield_regen");
   MELEMENT(temp->energy==0.,"energy");
   MELEMENT(temp->energy_regen==0.,"energy_regen");
   MELEMENT(temp->fuel==0.,"fuel");*/
   MELEMENT(temp->crew==0,"crew");
   MELEMENT(temp->mass==0.,"mass");
   MELEMENT(temp->fuel_consumption==0,"fuel_consumption");
   /*MELEMENT(temp->cap_cargo==0,"cargo");
   MELEMENT(temp->cpu==0.,"cpu");*/
#undef MELEMENT

   return 0;
}


/**
 * @brief Loads all the ships in the data files.
 *
 *    @return 0 on success.
 */
int ships_load (void)
{
   size_t bufsize, nfiles;
   char *buf, **ship_files, *file;
   int i, sl;
   xmlNodePtr node;
   xmlDocPtr doc;

   /* Validity. */
   ss_check();

   ship_files = ndata_list( SHIP_DATA_PATH, &nfiles );

   /* Initialize stack if needed. */
   if (ship_stack == NULL) {
      ship_stack = array_create_size(Ship, nfiles);
   }

   for (i=0; i<(int)nfiles; i++) {

      /* Get the file name .*/
      sl   = strlen(SHIP_DATA_PATH)+strlen(ship_files[i])+1;
      file = malloc( sl );
      nsnprintf( file, sl, "%s%s", SHIP_DATA_PATH, ship_files[i] );

      /* Load the XML. */
      buf  = ndata_read( file, &bufsize );
      doc  = xmlParseMemory( buf, bufsize );

      if (doc == NULL) {
         free(buf);
         WARN(_("%s file is invalid xml!"), file);
         free(file);
         continue;
      }

      node = doc->xmlChildrenNode; /* First ship node */
      if (node == NULL) {
         xmlFreeDoc(doc);
         free(buf);
         WARN(_("Malformed %s file: does not contain elements"), file);
         free(file);
         continue;
      }

      free(file);

      if (xml_isNode(node, XML_SHIP))
         /* Load the ship. */
         ship_parse( &array_grow(&ship_stack), node );

      /* Clean up. */
      xmlFreeDoc(doc);
      free(buf);
   }

   /* Shrink stack. */
   array_shrink(&ship_stack);
   DEBUG( ngettext( "Loaded %d Ship", "Loaded %d Ships", array_size(ship_stack) ), array_size(ship_stack) );

   /* Clean up. */
   for (i=0; i<(int)nfiles; i++)
      free( ship_files[i] );
   free( ship_files );

   return 0;
}


/**
 * @brief Frees all the ships.
 */
void ships_free (void)
{
   Ship *s;
   int i, j;
   for (i = 0; i < array_size(ship_stack); i++) {
      s = &ship_stack[i];

      /* Free stored strings. */
      free(s->name);
      free(s->description);
      free(s->gui);
      free(s->base_type);
      free(s->fabricator);
      free(s->license);
      free(s->desc_stats);

      /* Free outfits. */
      for (j=0; j<s->outfit_nstructure; j++)
         outfit_freeSlot( &s->outfit_structure[j].slot );
      for (j=0; j<s->outfit_nutility; j++)
         outfit_freeSlot( &s->outfit_utility[j].slot );
      for (j=0; j<s->outfit_nweapon; j++)
         outfit_freeSlot( &s->outfit_weapon[j].slot );
      if (s->outfit_structure != NULL)
         free(s->outfit_structure);
      if (s->outfit_utility != NULL)
         free(s->outfit_utility);
      if (s->outfit_weapon != NULL)
         free(s->outfit_weapon);

      /* Free stats. */
      if (s->stats != NULL)
         ss_free( s->stats );

      /* Free graphics. */
      gl_freeTexture(s->gfx_space);
      if (s->gfx_engine != NULL)
         gl_freeTexture(s->gfx_engine);
      if (s->gfx_target != NULL)
         gl_freeTexture(s->gfx_target);
      if (s->gfx_store != NULL)
         gl_freeTexture(s->gfx_store);
      free(s->gfx_comm);
      for (j=0; j<s->gfx_noverlays; j++)
         gl_freeTexture(s->gfx_overlays[j]);
      if (s->gfx_overlays != NULL)
         free(s->gfx_overlays);

      /* Free collision polygons. */
      if (s->npolygon != 0) {
         for (j=0; j<s->npolygon; j++) {
            free(s->polygon[j].x);
            free(s->polygon[j].y);
         }
         free(s->polygon);
      }
   }

   array_free(ship_stack);
   ship_stack = NULL;
}
