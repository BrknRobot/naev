/*
 * See Licensing and Copyright notice in naev.h
 */



#ifndef WEAPON_H
#  define WEAPON_H


#include "outfit.h"
#include "physics.h"
#include "pilot.h"


/**
 * @enum WeaponLayer
 * @brief Designates the layer the weapon is on.
 * Automatically set up on creation (player is front, rest is back).
 */
typedef enum { WEAPON_LAYER_BG, WEAPON_LAYER_FG } WeaponLayer;

/**
 * @struct Weapon
 *
 * @brief In-game representation of a weapon.
 */
typedef struct Weapon_ {
   Solid *solid; /**< Actually has its own solid :) */
   unsigned int ID; /**< Only used for beam weapons. */

   int faction; /**< faction of pilot that shot it */
   unsigned int parent; /**< pilot that shot it */
   unsigned int target; /**< target to hit, only used by seeking things */
   const Outfit* outfit; /**< related outfit that fired it or whatnot */

   double real_vel; /**< Keeps track of the real velocity. */
   double jam_power; /**< Power being jammed by. */
   double dam_mod; /**< Damage modifier. */
   int voice; /**< Weapon's voice. */
   double exp_timer; /**< Explosion timer for beams. */
   double life; /**< Total life. */
   double timer; /**< mainly used to see when the weapon was fired */
   double anim; /**< Used for beam weapon graphics and others. */
   int sprite; /**< Used for spinning outfits. */
   const PilotOutfitSlot *mount; /**< Used for beam weapons. */
   double falloff; /**< Point at which damage falls off. */
   double strength; /**< Calculated with falloff. */
   int sx; /**< Current X sprite to use. */
   int sy; /**< Current Y sprite to use. */

   /* position update and render */
   void (*update)(struct Weapon_*, const double, WeaponLayer); /**< Updates the weapon */
   void (*think)(struct Weapon_*, const double); /**< for the smart missiles */

   char status; /**< Weapon status - to check for jamming */
} Weapon;

/*
 * addition
 */
void weapon_add( const Outfit* outfit, const double T,
      const double dir, const Vector2d* pos, const Vector2d* vel,
      const Pilot *parent, const unsigned int target );


/*
 * Beam weapons.
 */
unsigned int beam_start( const Outfit* outfit,
      const double dir, const Vector2d* pos, const Vector2d* vel,
      const Pilot *parent, const unsigned int target,
      const PilotOutfitSlot *mount );
void beam_end( const unsigned int parent, unsigned int beam );


/*
 * Misc stuff.
 */
void weapon_explode( double x, double y, double radius,
      int dtype, double damage,
      const Pilot *parent, int mode );


/*
 * update
 */
void weapons_update( const double dt );
void weapons_render( const WeaponLayer layer, const double dt );


/*
 * clean
 */
void weapon_clear (void);
void weapon_exit (void);


#endif /* WEAPON_H */

