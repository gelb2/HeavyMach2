"""Bounded offline adapter around existing C++ image/table readers."""
from pathlib import Path
import fcntl
import hashlib
import json
import struct
import subprocess

def tool(root):
    root=Path(root).resolve();home=root/'.local/presentation-harness/reader';home.mkdir(parents=True,exist_ok=True)
    sources=[root/'.agents/harness/inspect_original.cpp',root/'Sources/Assets/Image.cpp',root/'Sources/Assets/Image.hpp',root/'Sources/Core/Data/DataTable.cpp']
    sources+=list((root/'Sources/Core/Data').glob('*.hpp'))+list((root/'ThirdParty/stb').glob('*.h'))
    expected=hashlib.sha256(b''.join(p.as_posix().encode()+p.read_bytes() for p in sorted(sources))).hexdigest()
    binary=home/'inspect-original';manifest=home/'build.json'
    with (home/'lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        if binary.is_symlink() or manifest.is_symlink():raise ValueError('Linked harness reader')
        previous=json.loads(manifest.read_text()) if manifest.exists() else {}
        if not binary.exists() or previous.get('source')!=expected or hashlib.sha256(binary.read_bytes()).hexdigest()!=previous.get('binary'):
            temporary=home/'inspect-original.new'
            command=['clang++','-std=c++20','-O2','-I',str(root/'Sources'),'-I',str(root/'ThirdParty/stb'),str(sources[0]),str(root/'Sources/Assets/Image.cpp'),str(root/'Sources/Core/Data/DataTable.cpp'),'-lz','-o',str(temporary)]
            compile=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            (home/'compile.log').write_bytes(compile.stdout)
            if compile.returncode:raise ValueError('Shared reader build failed; see local compile.log')
            temporary.replace(binary);manifest.write_text(json.dumps({'source':expected,'binary':hashlib.sha256(binary.read_bytes()).hexdigest()})+'\n')
    return binary

def image(root,path):
    result=subprocess.run([str(tool(root)),'image'],input=Path(path).read_bytes(),stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if result.returncode or len(result.stdout)<8:raise ValueError('Shared image decoder rejected input')
    w,h=struct.unpack('<II',result.stdout[:8]);pixels=result.stdout[8:]
    if len(pixels)!=w*h*4:raise ValueError('Shared decoder length mismatch')
    return w,h,4,[pixels[y*w*4:(y+1)*w*4] for y in range(h)]

def table(root,data):
    result=subprocess.run([str(tool(root)),'table'],input=data,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if result.returncode:raise ValueError('Shared table reader rejected input')
    return json.loads(result.stdout)
