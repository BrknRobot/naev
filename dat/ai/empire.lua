require 'ai.core.core'
local fmt = require "format"

-- Settings
mem.armour_run    = 40
mem.armour_return = 70
mem.aggressive    = true
mem.whiteknight   = true

local bribe_no_list = {
   _([["You won't buy your way out of this one."]]),
   _([["The Empire likes to make examples out of scum like you."]]),
   _([["You've made a huge mistake."]]),
   _([["Bribery carries a harsh penalty, scum."]]),
   _([["I'm not interested in your blood money!"]]),
   _([["All the money in the world won't save you now!"]])
}
local taunt_list_offensive = {
   _("There is no room in this universe for scum like you!"),
   _("The Empire will enjoy your death!"),
   _("Your head will make a fine gift for the Emperor!"),
   _("None survive the wrath of the Emperor!"),
   _("Enjoy your last moments, criminal!")
}
local taunt_list_defensive = {
   _("You dare attack me?!"),
   _("You are no match for the Empire!"),
   _("The Empire will have your head!"),
   _("You'll regret that!"),
   _("That was a fatal mistake!")
}

function create ()
   local p = ai.pilot()
   local ps = p:ship()

   -- Not too many credits.
   ai.setcredits( rnd.rnd(ps:price()/300, ps:price()/70) )

   -- Lines to annoy the player. Shouldn't be too common or Gamma Polaris and such get inundated.
   local r = rnd.rnd(0,20)
   if r == 0 then
      p:broadcast(_("The Empire is watching you."))
   elseif r == 1 then
      p:broadcast(_("The Emperor sees all."))
   end

   mem.loiter = 3 -- This is the amount of waypoints the pilot will pass through before leaving the system

   -- Set how far they attack
   mem.enemyclose = 3000 * ps:size()

   -- Finish up creation
   create_post()
end

-- When hailed
function hail ()
   local p = ai.pilot()

   -- Remove randomness from future calls
   if not mem.hailsetup then
      mem.refuel_base = rnd.rnd( 2000, 4000 )
      mem.bribe_base = math.sqrt( ai.pilot():stats().mass ) * (500 * rnd.rnd() + 1750)
      mem.bribe_rng = rnd.rnd()
      mem.hailsetup = true
   end

   -- Clean up
   mem.refuel        = 0
   mem.refuel_msg    = nil
   mem.bribe         = 0
   mem.bribe_prompt  = nil
   mem.bribe_prompt_nearby = nil
   mem.bribe_paid    = nil
   mem.bribe_no      = nil

   -- Get refuel chance
   local standing = p:faction():playerStanding()
   mem.refuel = mem.refuel_base
   if standing < 0 then
      mem.refuel_no = _([["My fuel is property of the Empire."]])
   elseif standing < 40 then
      if rnd.rnd() > 0.8 then
         mem.refuel_no = _([["My fuel is property of the Empire."]])
      end
   else
      mem.refuel = mem.refuel * 0.6
   end
   -- Most likely no chance to refuel
   mem.refuel_msg = string.format( _([["I suppose I could spare some fuel for %s, but you'll have to do the paperwork."]]), fmt.credits(mem.refuel) )

   -- See if can be bribed
   mem.bribe = mem.bribe_base
   if (mem.natural or mem.allowbribe) and (standing > 0 or
         (standing > -20 and mem.bribe_rng > 0.7) or
         (standing > -50 and mem.bribe_rng > 0.5) or
         (mem.bribe_rng > 0.3)) then
      mem.bribe_prompt = string.format(_([["For some %s I could forget about seeing you."]]), fmt.credits(mem.bribe) )
      mem.bribe_paid = _([["Now scram before I change my mind."]])
   else
      mem.bribe_no = bribe_no_list[ rnd.rnd(1,#bribe_no_list) ]
   end
end

-- taunts
function taunt ( target, offense )
   -- Only 50% of actually taunting.
   if rnd.rnd(0,1) == 0 then
      return
   end

   -- some taunts
   local taunts
   if offense then
      taunts = taunt_list_offensive
   else
      taunts = taunt_list_defensive
   end

   ai.pilot():comm(target, taunts[ rnd.rnd(1,#taunts) ])
end
