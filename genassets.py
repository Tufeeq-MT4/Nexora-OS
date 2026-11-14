import struct, math
from PIL import Image, ImageDraw, ImageFont
P='/usr/share/fonts/truetype/google-fonts/Poppins-Regular.ttf'
def mkfont(px, first, last, name):
    f=ImageFont.truetype(P, px)
    n=last-first+1
    glyphs=[]; blobs=[]
    off=8+n*16
    for c in range(first,last+1):
        ch=chr(c)
        adv=int(round(f.getlength(ch)))
        bbox=f.getbbox(ch, anchor='ls')   # relative to baseline origin
        if ch==' ' or bbox[2]<=bbox[0]:
            glyphs.append((0,0,0,0,0,adv,0)); continue
        w=bbox[2]-bbox[0]; h=bbox[3]-bbox[1]
        im=Image.new('L',(w,h),0)
        d=ImageDraw.Draw(im); d.text((-bbox[0],-bbox[1]),ch,font=f,fill=255,anchor='ls')
        data=im.tobytes()
        glyphs.append((off,w,h,bbox[0],bbox[1],adv,0)); blobs.append(data); off+=len(data)
    out=struct.pack('<II',n,first)
    for g in glyphs: out+=struct.pack('<IHHhhHH',*g)
    for b in blobs: out+=b
    open(name,'wb').write(out)
mkfont(22,32,126,'font0.bin'); mkfont(34,32,126,'font1.bin'); mkfont(128,48,58,'font2.bin')
# logo: 2 hexagon-ish blobs, palette indexed
S=200; w=S; h=int(S*467/400)
im=Image.new('P',(w,h),0)
pal=[0]*1024
pal[4:8]=[0x3a,0x6f,0xc4,255]; pal[8:12]=[0xf0,0xf2,0xf5,255]; pal[12:16]=[0x10,0x14,0x1a,255]
im2=Image.new('RGBA',(w,h),(0,0,0,0)); d=ImageDraw.Draw(im2)
def hexa(cx,cy,r,col):
    pts=[(cx+r*math.cos(math.radians(60*i+30)),cy+r*math.sin(math.radians(60*i+30))) for i in range(6)]
    d.polygon(pts,fill=col)
hexa(w*0.35,h*0.5,w*0.38,(0x3a,0x6f,0xc4,255)); hexa(w*0.65,h*0.5,w*0.38,(0xf0,0xf2,0xf5,255))
hexa(w*0.35,h*0.5,w*0.2,(0x10,0x14,0x1a,255)); hexa(w*0.65,h*0.5,w*0.2,(0x10,0x14,0x1a,255))
px=im2.load(); idx=bytearray()
cmap={(0x3a,0x6f,0xc4,255):1,(0xf0,0xf2,0xf5,255):2,(0x10,0x14,0x1a,255):3}
for y in range(h):
    for x in range(w):
        p=px[x,y]; idx.append(cmap.get(p,0))
open('logo.bin','wb').write(struct.pack('<HH',w,h)+bytes(pal)+bytes(idx))
print('assets ok')
