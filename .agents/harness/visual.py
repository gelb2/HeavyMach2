"""Validate actual renderer observations, original provenance and PNG evidence.

Geometry must come from the candidate's real frame, not a success mask. Unknown
coverage is a failure; black source artwork is not automatically a letterbox.
"""
from pathlib import Path
import hashlib
import math
import struct
import zlib
from provenance import original_catalog

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def png_rgb(path):
    data=Path(path).read_bytes()
    if data[:8]!=b'\x89PNG\r\n\x1a\n':raise ValueError('Not PNG')
    at=8;compressed=b'';header=None;ended=False
    while at<len(data):
        size=struct.unpack('>I',data[at:at+4])[0];kind=data[at+4:at+8];block=data[at+8:at+8+size];crc=data[at+8+size:at+12+size]
        if len(crc)!=4 or zlib.crc32(kind+block)&0xffffffff!=struct.unpack('>I',crc)[0]:raise ValueError('PNG CRC failure')
        if kind==b'IHDR':header=struct.unpack('>IIBBBBB',block)
        if kind==b'IDAT':compressed+=block
        at+=12+size
        if kind==b'IEND':ended=True;break
    if not header or not ended:raise ValueError('Incomplete PNG')
    width,height,depth,color,compression,filtering,interlace=header
    if depth!=8 or color not in (2,6) or compression or filtering or interlace or not 0<width*height<=50_000_000:raise ValueError('Unsupported PNG layout')
    channels=3 if color==2 else 4;stride=width*channels
    raw=zlib.decompress(compressed)
    if len(raw)!=(stride+1)*height:raise ValueError('PNG length mismatch')
    rows=[];previous=bytearray(stride)
    for y in range(height):
        start=y*(stride+1);mode=raw[start];row=bytearray(raw[start+1:start+1+stride])
        if mode>4:raise ValueError('Invalid PNG filter')
        for x in range(stride):
            left=row[x-channels] if x>=channels else 0;up=previous[x];upperleft=previous[x-channels] if x>=channels else 0
            if mode==1:v=left
            elif mode==2:v=up
            elif mode==3:v=(left+up)//2
            elif mode==4:
                p=left+up-upperleft;dist=[abs(p-left),abs(p-up),abs(p-upperleft)];v=(left,up,upperleft)[dist.index(min(dist))]
            else:v=0
            row[x]=(row[x]+v)&255
        rows.append(bytes(row));previous=row
    return width,height,channels,rows

def black_edge_bands(path):
    width,height,c,rows=png_rgb(path)
    # Candidate detection only. Geometry/source correspondence decides rejection.
    def dark(x,y):return max(rows[y][x*c:x*c+3])<=3
    column=[all(dark(x,y) for y in range(height)) for x in range(width)]
    line=[all(dark(x,y) for x in range(width)) for y in range(height)]
    def edges(values):
        a=next((i for i,v in enumerate(values) if not v),len(values));b=next((i for i,v in enumerate(reversed(values)) if not v),len(values));return [a,b]
    return {'horizontal':edges(line),'vertical':edges(column),'width':width,'height':height}

def validate_observations(report,root):
    root=Path(root).resolve();failures=[];cases=report.get('cases',[])
    try:catalog=original_catalog(root)
    except (OSError,KeyError,ValueError) as error:return {'status':'FAIL','cases':len(cases),'failures':['Original provenance unavailable: '+str(error)]}
    if report.get('origin')!='candidate-native-renderer':failures.append('native renderer origin missing')
    if not report.get('binarySha256') or not report.get('candidate'):failures.append('candidate/binary identity missing')
    if not cases:failures.append('no captured contexts')
    for case in cases:
        label=str(case.get('id','unknown'));errors=[]
        try:
            png=root/case['png'];expected=case['pngSha256']
            if not png.resolve().is_relative_to(root) or png.is_symlink() or sha(png)!=expected:raise ValueError('changed/outside capture')
            w,h,png_channels,png_rows=png_rgb(png)
            if case['drawable']!=[w,h]:errors.append('drawable/capture mismatch')
            if not case.get('actualInputResults') or any(not v.get('passed') for v in case['actualInputResults']):errors.append('actual input/state verification missing or failed')
            required=case.get('requiredContent',[])
            if not required:errors.append('no required content geometry')
            spans=[[] for _ in range(h)];decoded=[]
            for draw in required:
                # Entire-source correspondence is mandatory for hub/HQ/shop.
                resource=draw['resource'];p=root/'.local/prepared/HD'/resource
                if not p.resolve().is_relative_to((root/'.local/prepared/HD').resolve()) or p.is_symlink() or sha(p)!=draw['resourceSha256']:raise ValueError('wrong resource bytes')
                original=catalog.records.get(resource)
                if not original or original['sha256']!=draw['resourceSha256']:raise ValueError('wrong original edition/source bytes')
                if original.get('pixels')!=draw['sourcePixels']:raise ValueError('source pixel dimensions do not match original')
                profile=case['device'];role=draw['role']
                phone=resource.startswith('PhoneMenu/')
                if role=='backdrop' and phone!=(profile=='iphone'):errors.append('wrong device backdrop')
                if role=='backdrop' and resource!=catalog.backdrop(profile,case['base'],case['screen']):errors.append('wrong original base/screen backdrop reference')
                if role=='facility':
                    reference=catalog.base_refs.get((profile,case['base']),{}).get((draw['tableType'],draw['tableId']))
                    if reference!=resource:errors.append('wrong original facility/button table reference')
                if role=='product' and phone:errors.append('product artwork must be HD')
                if role not in {'backdrop','product','required-control','facility'}:errors.append('unapproved image role; generated text needs its own bitmap observation')
                iw,ih=map(float,draw['sourcePixels']);columns,rows,tile=map(int,draw.get('tile',[1,1,0]))
                if columns<1 or rows<1 or not 0<=tile<columns*rows:raise ValueError('invalid original tile index')
                cell=[(tile%columns)*iw/columns,(tile//columns)*ih/rows,iw/columns,ih/rows]
                source_rect=draw.get('sourceRect',cell)
                if source_rect!=cell:errors.append('source region differs from original tile')
                storage=draw.get('storagePixels',[iw,ih]);sw,sh=map(float,storage)
                if sw<iw or sh<ih or (sw!=iw and sw!=2**math.ceil(math.log2(iw))) or (sh!=ih and sh!=2**math.ceil(math.log2(ih))):raise ValueError('invalid original texture storage')
                expected_uv=[cell[0]/sw,cell[1]/sh,(cell[0]+cell[2])/sw,(cell[1]+cell[3])/sh]
                if len(draw['uv'])!=4 or any(not math.isclose(a,b,abs_tol=1e-6) for a,b in zip(draw['uv'],expected_uv)):errors.append('tile UV does not match original source/storage')
                x,y,rw,rh=map(float,draw['rect']);sx,sy=rw/cell[2],rh/cell[3]
                if not all(math.isfinite(v) for v in (x,y,rw,rh,iw,ih)) or min(rw,rh,iw,ih)<=0:raise ValueError('invalid geometry')
                if not math.isclose(sx,sy,rel_tol=1e-5,abs_tol=1e-7):errors.append('nonuniform image scale')
                if (role=='backdrop' and (columns,rows,tile)!=(1,1,0)) or draw.get('edgeReplication',False):errors.append('backdrop clipping/edge replication')
                if x<-.01 or y<-.01 or x+rw>w+.01 or y+rh>h+.01:errors.append('required content outside drawable')
                # Only artwork backdrops provide canvas coverage. Small real
                # controls cannot legitimize a large black filler panel.
                if role=='backdrop':
                    for row in range(max(0,math.ceil(y-.5)),min(h,math.ceil(y+rh-.5))):spans[row].append((max(0,math.ceil(x-.5)),min(w,math.ceil(x+rw-.5))))
                try:source_image=png_rgb(p)
                except (ValueError,zlib.error):
                    from common_decode import image
                    source_image=image(root,p)
                decoded.append((draw,cell,source_image))
            uncovered=0
            for row in spans:
                end=0
                for start,stop in sorted(row):
                    if start>end:uncovered+=start-end
                    end=max(end,stop)
                uncovered+=w-end
            if uncovered:errors.append('unmapped canvas pixels: '+str(uncovered))
            broken_black=0
            # Source correspondence, not a global black-pixel percentage. A
            # black original remains valid. Conservative four-neighbor samples
            # avoid treating normal filtering of art edges as missing pixels.
            for y in range(h):
                for x in range(w):
                    if max(png_rows[y][x*png_channels:x*png_channels+3])>3:continue
                    expected=None
                    for draw,cell,(dw,dh,dc,dr) in decoded:
                        dx,dy,rw,rh=draw['rect']
                        if not (dx<=x+.5<dx+rw and dy<=y+.5<dy+rh):continue
                        u=cell[0]+(x+.5-dx)/rw*cell[2]-.5;v=cell[1]+(y+.5-dy)/rh*cell[3]-.5
                        points=[(max(0,min(dw-1,int(math.floor(u))+ox)),max(0,min(dh-1,int(math.floor(v))+oy))) for ox,oy in [(0,0),(0,1),(1,0),(1,1)]]
                        colors=[dr[py][px*dc:px*dc+dc] for px,py in points]
                        opaque=dc==3 or all(c[3]==255 for c in colors)
                        bright=all(max(c[:3])>8 for c in colors)
                        if opaque:expected=bright
                        elif dc==4 and all(c[3]==0 for c in colors):pass
                        elif bright:expected=True
                        else:expected=None # Partially opaque dark edges are not a calibrated black-error probe.
                    if expected:broken_black+=1
            if broken_black:errors.append('source-corresponding bright pixels rendered black: '+str(broken_black))
            if case.get('fillerPanels'):errors.append('filler panels')
            if case.get('verticalScroll') or case.get('revealButtonUsed'):errors.append('scroll/reveal workaround')
            if case.get('layoutReference')!=('phone-original' if case['device']=='iphone' else 'hd-original'):errors.append('incorrect layout reference')
            if case.get('resourceTraceComplete') is not True:errors.append('actual texture provenance missing')
        except (ValueError,KeyError,TypeError,ZeroDivisionError,OSError,struct.error,zlib.error) as error:errors.append(str(error))
        failures.extend(label+': '+error for error in errors)
    return {'status':'FAIL' if failures else 'PASS','cases':len(cases),'failures':failures}
