#!/usr/bin/env python3
"""Run the real runtime numeric formatter + ARM word-vararg reader without SDL.

The formatter, VaList, read_next_arg, and scalar GuestArg implementations are
extracted unchanged from this project's pinned local touchHLE source. Only guest
memory/CPU storage and unused Objective-C/string-service branches are doubled.
This is not a full guest/objc_msgSend execution test; the simulator reproducer
must still be repeated with the rebuilt reference host.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / '.local/compat-source/upstream/fresh-port/20-touchHLE-src/touchHLE'


def block(source, marker):
    start = source.index(marker)
    brace = source.index('{', start)
    depth, quote, escape, index = 0, None, False, brace
    while index < len(source):
        c = source[index]
        if quote:
            if escape:
                escape = False
            elif c == '\\':
                escape = True
            elif c == quote:
                quote = None
        elif c == '"':
            quote = c
        elif c == "'" and re.match(r"'(?:\\.|[^'\\])'", source[index:]):
            # A Rust char literal, not a lifetime.
            quote = c
        elif source.startswith('//', index):
            index = source.find('\n', index)
            if index < 0:
                raise ValueError(marker)
            continue
        elif source.startswith('/*', index):
            end = source.index('*/', index + 2)
            index = end + 2
            continue
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
        index += 1
    raise ValueError(f'Unbalanced source item: {marker}')


STORAGE = r'''
#![allow(dead_code,unused_imports,unused_variables,non_camel_case_types)]
use std::io::Write;
use std::marker::PhantomData;
macro_rules! log_dbg {($($x:tt)*)=>{()}}
macro_rules! log {($($x:tt)*)=>{()}}
macro_rules! msg {($($x:tt)*)=>{nil}}
type GuestUSize=u32;
struct Ptr<T,const M:bool>(u32,PhantomData<T>);
impl<T,const M:bool> Copy for Ptr<T,M>{}
impl<T,const M:bool> Clone for Ptr<T,M>{fn clone(&self)->Self{*self}}
impl<T,const M:bool> std::fmt::Debug for Ptr<T,M>{fn fmt(&self,f:&mut std::fmt::Formatter<'_>)->std::fmt::Result{write!(f,"{:x}",self.0)}}
impl<T,const M:bool> PartialEq for Ptr<T,M>{fn eq(&self,r:&Self)->bool{self.0==r.0}}
impl<T,const M:bool> Ptr<T,M>{const fn from_bits(v:u32)->Self{Self(v,PhantomData)}const fn null()->Self{Self::from_bits(0)}fn to_bits(self)->u32{self.0}fn is_null(self)->bool{self.0==0}fn cast<U>(self)->Ptr<U,M>{Ptr::from_bits(self.0)}}
impl<T,const M:bool> std::ops::Add<u32> for Ptr<T,M>{type Output=Self;fn add(self,v:u32)->Self{Self::from_bits(self.0+v*std::mem::size_of::<T>() as u32)}}
type ConstPtr<T>=Ptr<T,false>;type MutPtr<T>=Ptr<T,true>;type ConstVoidPtr=ConstPtr<u8>;type MutVoidPtr=MutPtr<u8>;type id=MutPtr<u8>;type unichar=u16;type wchar_t=u32;
#[allow(non_upper_case_globals)]const nil:id=Ptr::from_bits(0);
trait Read:Copy{fn read(bytes:&[u8])->Self;}
impl Read for u8{fn read(b:&[u8])->Self{b[0]}}
impl Read for u32{fn read(b:&[u8])->Self{u32::from_le_bytes(b[..4].try_into().unwrap())}}
struct Mem{bytes:Vec<u8>}
impl Mem{
 fn read<T:Read,const M:bool>(&self,p:Ptr<T,M>)->T{T::read(&self.bytes[p.0 as usize..])}
 fn bytes_at<const M:bool>(&self,p:Ptr<u8,M>,n:u32)->&[u8]{&self.bytes[p.0 as usize..(p.0+n) as usize]}
 fn cstr_at<const M:bool>(&self,p:Ptr<u8,M>)->&[u8]{let tail=&self.bytes[p.0 as usize..];&tail[..tail.iter().position(|b|*b==0).unwrap()]}
 fn cstr_at_utf8<const M:bool>(&self,p:Ptr<u8,M>)->Result<&str,&[u8]>{let tail=&self.bytes[p.0 as usize..];let b=&tail[..tail.iter().position(|b|*b==0).unwrap()];std::str::from_utf8(b).map_err(|_|b)}
 fn wcstr_at<const M:bool>(&self,_:Ptr<u32,M>)->String{panic!("unused wide-string service")}
}
struct Cpu{regs:[u32;16]}impl Cpu{fn regs_mut(&mut self)->&mut[u32]{&mut self.regs}}
struct Environment{mem:Mem,cpu:Cpu}
fn strlen(env:&mut Environment,p:ConstPtr<u8>)->u32{env.mem.cstr_at_utf8(p).unwrap().len() as u32}
const LC_CTYPE:i32=0;fn setlocale(_: &mut Environment,_:i32,_:ConstPtr<u8>)->ConstPtr<u8>{Ptr::from_bits(0)}
mod ns_string{pub fn to_rust_string(_: &mut super::Environment,_:super::id)->String{panic!("unused Objective-C object service")}}
'''

TESTS = r'''
fn run<const NS:bool>(format:&str,args:&[u32],fixed:usize)->String{
 let mut env=Environment{mem:Mem{bytes:vec![0;4096]},cpu:Cpu{regs:[0;16]}};
 env.mem.bytes[0]=b'C';let stack=128;
 for (n,word) in args.iter().enumerate(){let at=fixed+n;if at<4{env.cpu.regs[at]=*word;}else{let offset=stack+(at-4)*4;env.mem.bytes[offset..offset+4].copy_from_slice(&word.to_le_bytes());}}
 let input=format.as_bytes();let result=printf_inner::<NS,_>(&mut env,|_,i|input.get(i as usize).copied().unwrap_or(0),VaList{reg_offset:fixed,stack_pointer:Ptr::from_bits(stack as u32)});
 String::from_utf8(result).unwrap()
}
fn double(v:f64)->[u32;2]{let b=v.to_bits();[b as u32,(b>>32)as u32]}
#[test]fn hm2_signed_integer_deltas(){
 for (fmt,value,expected) in [("%+d",5,"+5"),("%+d",-5,"-5"),("%+d",0,"+0"),("%+8d",5,"      +5"),("%+08d",5,"+0000005"),("%+08d",-5,"-0000005"),("%+08d",0,"+0000000"),("%+.4d",12,"+0012"),("%+.4d",-12,"-0012"),("%+8.4d",12,"   +0012"),("%+08.4d",-12,"   -0012"),("%.0d",0,""),("%+.0d",0,"+"),("%+d",i32::MIN,"-2147483648")]{assert_eq!(run::<false>(fmt,&[value as u32],4),expected,"{} {}",fmt,value);}
 assert_eq!(run::<false>("%+lld",&[0,0x80000000],4),"-9223372036854775808");
 assert_eq!(run::<false>("%+hhd/%+hd/%+ld",&[0x80,0x8000,3],4),"-128/-32768/+3");
 assert_eq!(run::<false>("%u",&[u32::MAX],4),"4294967295");
}
#[test]fn hm2_original_sprintf_chk_stack_formats(){
 assert_eq!(run::<false>("Atk %d (L:%+d R:%+d)\n",&[20,5,(-3i32)as u32],4),"Atk 20 (L:+5 R:-3)\n");
 assert_eq!(run::<false>("HP : %d (%+d)\nDef : %d (%+d)\n\n",&[101,2,13,(-3i32)as u32],4),"HP : 101 (+2)\nDef : 13 (-3)\n\n");
 let mut args=vec![10,2];args.extend(double(0.125));args.extend(double(1.25));assert_eq!(run::<false>("%d / %d / %.2fs / %.1fs\n\n",&args,4),"10 / 2 / 0.12s / 1.2s\n\n");
}
#[test]fn signed_float_precision_and_negative_zero(){
 for (fmt,value,expected) in [("%+f",1.25,"+1.250000"),("%+f",-1.25,"-1.250000"),("%+f",0.0,"+0.000000"),("%+f",-0.0,"-0.000000"),("%+.2f",1.25,"+1.25"),("%+8.2f",1.25,"   +1.25"),("%+08.2f",1.25,"+0001.25"),("%+08.2f",-1.25,"-0001.25"),("%.2f",-1.25,"-1.25")]{assert_eq!(run::<false>(fmt,&double(value),4),expected,"{} {}",fmt,value);}
 let mut args=vec![2];args.extend(double(-1.25));args.push(7);assert_eq!(run::<false>("%+.*f/%+d",&args,4),"-1.25/+7");
}
#[test]fn unchanged_arm_packed_register_stack_varargs(){
 // Actual read_next_arg + VaList.next are extracted unchanged. ARM Darwin
 // packs words and can split a double between r3 and the stack; no new
 // host-ABI alignment or vararg decoder is introduced by the format patch.
 assert_eq!(run::<false>("%+d/%+d/%+d",&[7,(-2i32)as u32,0],2),"+7/-2/+0");
 let mut words=vec![7];words.extend(double(1.5));words.push((-2i32)as u32);assert_eq!(run::<true>("%+d/%+.2f/%+d",&words,3),"+7/+1.50/-2");
 let mut split=double(1.25).to_vec();split.push((-3i32)as u32);assert_eq!(run::<true>("%+.2f/%+d",&split,3),"+1.25/-3");
 assert_eq!(run::<false>("%+*d/%+d",&[6,5,9],4),"    +5/+9");
}
'''


def harness(printf, abi):
    constants = '\n'.join(re.search(rf'const {name}:[^=]+?=.*?;', printf, re.S).group(0)
                          for name in ['ALL_SPECIFIERS', 'INTEGER_SPECIFIERS', 'FLOAT_SPECIFIERS'])
    items = [block(abi, 'pub trait GuestArg:'), block(abi, 'fn read_next_arg<'),
             '#[derive(Copy,Clone,Debug)]\n' + block(abi, 'pub struct VaList '), block(abi, 'impl VaList '),
             block(abi, 'macro_rules! impl_GuestArg_with'), block(abi, 'impl GuestArg for u32 '),
             '\n'.join(f'impl_GuestArg_with!({kind},u32);' for kind in ['i32', 'u16', 'i16', 'u8', 'i8']),
             block(abi, 'impl<T, const MUT: bool> GuestArg for Ptr<T, MUT>'),
             block(abi, 'impl GuestArg for u64 '), 'impl_GuestArg_with!(i64,u64);',
             block(abi, 'impl GuestArg for f64 '), constants, block(printf, 'pub fn printf_inner<'),
             block(printf, 'fn f_format('), block(printf, 'fn e_format(')]
    if 'fn integer_format(' in printf:
        items.append(block(printf, 'fn integer_format('))
    return STORAGE + '\n'.join(items) + TESTS


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / '.local'):
        raise SystemExit('Regression outputs must remain in this HM2 project .local directory')
    output.mkdir(parents=True, exist_ok=True)
    printf_path, abi_path = RUNTIME / 'src/libc/stdio/printf.rs', RUNTIME / 'src/abi.rs'
    printf, abi = printf_path.read_text(), abi_path.read_text()
    source = output / 'printf-regression.rs'
    source.write_text(harness(printf, abi))
    binary = output / 'printf-regression'
    compile_command = [str(Path.home()/'.cargo/bin/rustc'), '+1.99.1', '--edition=2021', '--test', '-O', str(source), '-o', str(binary)]
    result = subprocess.run(compile_command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (output/'compile.log').write_text(result.stdout)
    if result.returncode:
        print(result.stdout)
        return result.returncode
    run = subprocess.run([str(binary), '--nocapture'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (output/'tests.log').write_text(run.stdout)
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    (output/'manifest.json').write_text(json.dumps({'printf_source_sha256':sha(printf_path),'abi_source_sha256':sha(abi_path),'harness_sha256':sha(source),'compile_exit':result.returncode,'tests_exit':run.returncode,'compile_command':compile_command,'scope':'actual extracted printf_inner and actual packed ARM VaList/read_next_arg; storage stubs only; no SDL/guest/ObjC dispatch execution'},indent=2)+'\n')
    print(run.stdout)
    return run.returncode


if __name__ == '__main__':
    raise SystemExit(main())
