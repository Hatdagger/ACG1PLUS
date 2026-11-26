import os
"""Run a game's compile routine in an emulator on a given build. Reads only the user's own disc files in the sandbox."""
from unicorn import *
from unicorn.mips_const import *
import struct
W=os.environ.get('ACG_WORK','work')+'/'
GAMES={
 'pp': dict(exe=W+'pp-disc/SLUS_006.70', mod=W+'pp-disc/unpacked/GG/COM/FDAT/201.bin', modbase=0x800490D0,
            compile=0x8009A238, loadout=0x800342B4, optmask=0x8003BCCC),
 'moa':dict(exe=W+'ma-disc/SLUS_010.30', mod=W+'ma-disc/unpacked/GG/COM/FDAT/206.bin', modbase=0x80050DD8,
            compile=0x800A7BB4, loadout=0x80036FB0, optmask=0x8003EA70),
}
STOP=0x80000100
P=lambda a:a&0x1FFFFFFF
class Game:
    def __init__(s,g):
        s.g=GAMES[g]; s.name=g
        s.exe=open(s.g['exe'],'rb').read(); s.mod=open(s.g['mod'],'rb').read()
    def fresh(s):
        mu=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32+UC_MODE_LITTLE_ENDIAN)
        mu.mem_map(0x0,0x200000)
        mu.mem_write(P(0x80011000),s.exe[0x800:])
        mu.mem_write(P(s.g['modbase']),s.mod)
        mu.mem_write(P(STOP),struct.pack('<I',0))
        return mu
    def run(s,loadout,opts=0,player=0,setup=None,track=False):
        mu=s.fresh()
        mu.mem_write(P(s.g['loadout']),bytes(loadout))
        mu.mem_write(P(s.g['optmask']),struct.pack('<H',opts))
        if setup: setup(mu)
        writes={}
        if track:
            def hk(uc,acc,addr,size,val,ud): writes[addr]=size
            mu.hook_add(UC_HOOK_MEM_WRITE,hk)
        mu.hook_add(UC_HOOK_CODE,lambda *a:None)
        mu.reg_write(UC_MIPS_REG_SP,0x801FFF00); mu.reg_write(UC_MIPS_REG_RA,STOP)
        mu.reg_write(UC_MIPS_REG_A0,player)
        mu.emu_start(s.g['compile'],STOP,count=3_000_000)
        return mu,writes
    def block(s,mu,player=0):
        base={'pp':0x80042740,'moa':0x800453E8}[s.name]+player*{'pp':936,'moa':928}[s.name]
        return bytes(mu.mem_read(P(base),0x150))
