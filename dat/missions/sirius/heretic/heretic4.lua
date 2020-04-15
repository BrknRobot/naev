--[[misn title - the egress]]
--[[this mission begins with the frenetic nasin wanting to escape
   the wringer due to being overwhelmed by house sirius. the player
   loads up with as many nasin as their vessel will carry, and takes
   them to seek refuge in the ingot system on planet ulios, where they
   begin to rebuild and plan.... (ominous music).
   this mission is designed to be the end of part 1, and is supposed
   to be very hard, and slightly combat oriented, but more supposed to
   involve smuggling elements.]]

--beginning messages
bmsg = {}
bmsg[1] = _([[You run up to Draga, who has a look of desperation on his face. "We need to go, now," he says. "The Sirii are overwhelming us, they're about to finish us off. Will you take me and as many Nasin as you can carry to %s in the %s system? This is our most desperate hour!"]])
bmsg[2] = _([["Thank you! I knew you would do it!" Draga then proceeds to file as many people as can possibly fit onto your ship, enough to fill your ship's cargo hold to the brim. The number of Nasin members shocks you as they are packed into your ship.
    As the Sirii approach ever closer, Draga yells at you to get the ship going and take off. As you get in the cabin and begin taking off, you see Draga under fire by a Sirian soldier. The last thing you see as you take off is Draga laying on the ground, lifeless.]])

--ending messages
emsg = {}
emsg[1] = _([[You land on %s, and you open the bay doors. You are still amazed at how many people Draga had helped get into the cargo hold. As you help everyone out of your ship, a man walks up to you. "Hello, my name is Jimmy. Thank you for helping all of these people. I am grateful. I've heard about you from Draga, and I will be forever in your debt. Here, please, take this." He presses a credit chip in your hand just as you finish helping everyone out of your ship. It seems it was a job well done.]])

--mission osd
osd = {}
osd[1] = _("Fly the refugees to %s in the %s system.")

--random odds and ends
abort_msg = _([[You decide that this mission is just too much. You open up the cargo doors and jettison all %s people out into the cold emptiness of space. The Nasin will hate you forever, but you did what you had to do.]])
misn_title = _("The Egress")
npc_name = _("Draga")
bar_desc = _("Draga is running around, helping the few Nasin in the bar to get stuff together and get out.")
misn_desc = _("Assist the Nasin refugees by flying to %s in %s, and unloading them there.")
misn_reward = _("%s credits")

function create()
   --this mission make no system claims.
   --initalize your variables
   nasin_rep = faction.playerStanding("Nasin")
   misn_tracker = var.peek("heretic_misn_tracker")
   reward = math.floor((100000+(math.random(5,8)*2000)*(nasin_rep^1.315))*.01+.5)/.01
   homeasset = planet.cur()
   targetasset, targetsys = planet.get("Ulios") --this will be the new HQ for the nasin in the next part.
   people_carried =  (16 * free_cargo) + 7 --average weight per person is 62kg. one ton / 62 is 16. added the +7 for ships with 0 cargo.
   --set some mission stuff
   misn.setNPC(npc_name,"neutral/thief2")
   misn.setDesc(bar_desc)
   misn.setTitle(misn_title)
   misn.setReward(misn_reward:format(numstring(reward)))

   osd[1] = osd[1]:format(targetasset:name(),targetsys:name())
end

function accept()
   --inital convo. Kept it a yes no to help with the urgent feeling of the situation.

   local msg = bmsg[1]:format( targetasset:name(), targetsys:name() )
   if not tk.yesno(misn_title, msg) then
      misn.finish ()
   end
   misn.accept()
   misn.setDesc(misn_desc:format( targetasset:name(), targetsys:name()))
   misn.osdCreate(misn_title,osd)
   misn.osdActive(1)
   player.allowSave(false) -- so the player won't get stuck with a mission they can't complete.
   tk.msg(misn_title,bmsg[2])
   tk.msg(misn_title,bmsg[3])
   --convo over. time to finish setting the mission stuff.
   misn.markerAdd(targetsys,"high")
   free_cargo = player.pilot():cargoFree()
   people_carried =  (16 * free_cargo) + 7 --average weight per person is 62kg. one ton / 62 is 16. added the +7 for ships with 0 cargo.
   refugees = misn.cargoAdd("Refugees",free_cargo)
   player.takeoff()
   --get the hooks.
   hook.takeoff("takeoff")
   hook.jumpin("attacked")
   hook.jumpout("lastsys")
   hook.land("misn_over")
end

function takeoff()
   pilot.add("Sirius Assault Force",sirius,vec2.new(rnd.rnd(-450,450),rnd.rnd(-450,450))) --left over fleets from the prior mission.
   pilot.add("Sirius Assault Force",sirius,vec2.new(rnd.rnd(-450,450),rnd.rnd(-450,450)))
   pilot.add("Sirius Assault Force",sirius,vec2.new(rnd.rnd(-450,450),rnd.rnd(-450,450)))
   pilot.add("Nasin Sml Civilian",nil,homeasset) --other escapees.
   pilot.add("Nasin Sml Civilian",nil,homeasset)
   pilot.add("Nasin Sml Civilian",nil,homeasset)
   pilot.add("Nasin Sml Attack Fleet",nil,homeasset) --these are trying to help.
   pilot.add("Nasin Sml Attack Fleet",nil,homeasset)
end

function lastsys()
   last_sys_in = system.cur()
end

function attacked() --several systems where the sirius have 'strategically placed' an assault fleet to try and kill some nasin.
   dangersystems = {
   system.get("Neon"),
   system.get("Pike"),
   system.get("Vanir"),
   system.get("Aesir"),
   system.get("Herakin"),
   system.get("Eiderdown"),
   system.get("Eye of Night"),
   system.get("Lapis"),
   system.get("Ruttwi"),
   system.get("Esker"),
   system.get("Gutter")
   }
   for i,sys in ipairs(dangersystems) do
      if system.cur() == sys then
         pilot.add("Sirius Assault Force",sirius,vec2.new(rnd.rnd(-300,300),rnd.rnd(-300,300)))
      end
   end
   local chance_help,chance_civvie = rnd.rnd(1,3),rnd.rnd(1,3) --attack fleet and civvies are meant as a distraction to help the player.
   if chance_help == 1 then
      pilot.add("Nasin Sml Attack Fleet",nil,last_sys_in)
   end
   for i = 1,chance_civvie do
      pilot.add("Nasin Sml Civilian",nil,last_sys_in)
   end
end

function misn_over() --arent you glad thats over?
   if planet.cur() == planet.get("Ulios") then
      --introing one of the characters in the next chapter.
      tk.msg(misn_title,emsg[1]:format( targetasset:name() ))
      tk.msg(misn_title,emsg[2])
      player.pay(reward)
      misn.cargoRm(refugees)
      misn_tracker = misn_tracker + 1
      faction.modPlayer("Nasin",25) --big boost to the nasin, for completing the prologue
      var.push("heretic_misn_tracker",misn_tracker)
      misn.osdDestroy()
      player.allowSave(true)
      misn.finish(true)
   end
end

function abort()
   tk.msg(misn_title,abort_msg:format(numstring(people_carried)))
   misn.cargoJet(refugees)
   var.push("heretic_misn_tracker",-1) --if the player jettisons the peeps, the nasin will not let the player join there ranks anymore.
   faction.modPlayerSingle("Nasin",-200) --big boost to the nasin, for completing the prologue
   player.allowSave(true)
   misn.finish(true)
end
