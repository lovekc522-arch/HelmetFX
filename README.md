Helmet FX mod - documentation included below (once I write it), beta build.

This mod currently allows server admins, mission makers or clients to determine helmet effects per individual helmet using effect chains. Effects can be called in any order, and can be run multiple times to achieve the desired effect. A large swath of effects are currently supported. Voice effects turn on and off as helmets are removed.

Clients will need to install the Teamspeak 3 plugin found within the files if they want their voice to be modulated. You do not need this mod or the plugin to hear the effect of the mod, as the effect is handled on the client prior to broadcast within the Teamspeak 3 audio pipeline.

To my knowledge, this does not conflict with TFAR in any way, although some audio effects may behave strangely while used on a radio.

This mod is currently APL-ND, but once out of beta it will move to APL-SA, as all my mods do.


STAGES (use these names in the chain string):
lp low-pass hz, q
hp high-pass hz, q
bp band-pass hz, q
eq peaking EQ hz, q, db
ls low shelf hz, q, db
hs high shelf hz, q, db
gain volume db
comp compressor thr, ratio, atk, rel, makeup
dist distortion drive, mode(0 soft,1 hard,2 fold), mix, outdb
crush bitcrusher bits, down, mix
ring ring modulator hz, mix
comb comb/flanger ms, fb, mix
reverb room reverb size, damp, mix
pitch pitch shifter semi, win, mix
noise breath/hiss type(0 white,1 soft), db, breath, depth, gate
limit safety limiter ceil

Chain string example:
base_helmet=comp:thr=-18,ratio=4|hp:hz=300|lp:hz=3200|dist:drive=3,mix=0.4|limit:ceil=-1;

Separate effects with the | symbol, attributes of an effect with , and between helmets with ;
