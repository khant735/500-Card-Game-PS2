#!/usr/bin/env python3
from pathlib import Path
import argparse, struct
from voice_catalog import CLIPS
from language_catalog import BY_FOLDER

ap=argparse.ArgumentParser()
ap.add_argument('--language', required=True)
ap.add_argument('--gender', required=True, choices=['MALE','FEMALE'])
ap.add_argument('--region-id', type=int, default=0)
ap.add_argument('--input', required=True, type=Path)
ap.add_argument('--output', required=True, type=Path)
ap.add_argument('--max-sample', type=int, default=131072)
a=ap.parse_args()
lang=a.language.upper()
if lang not in BY_FOLDER or lang=='AUTO': raise SystemExit(f'Unknown language folder: {lang}')
lang_id=BY_FOLDER[lang]
gender=0 if a.gender=='MALE' else 1
header_size=36; index_size=len(CLIPS)*8; cursor=header_size+index_size
entries=[]; payload=bytearray()
for name,_ in CLIPS:
    f=a.input/f'{name}.adp'
    if not f.exists(): entries.append((0,0)); continue
    b=f.read_bytes()
    if len(b)>a.max_sample: raise SystemExit(f'{f}: {len(b)} exceeds {a.max_sample}-byte runtime sample limit')
    entries.append((cursor,len(b))); payload.extend(b); cursor+=len(b)
a.output.parent.mkdir(parents=True, exist_ok=True)
with a.output.open('wb') as f:
    f.write(struct.pack('<4s7I',b'VPK4',4,len(CLIPS),gender,a.region_id,len(CLIPS),header_size,index_size))
    f.write(struct.pack('<I',lang_id))
    for off,size in entries: f.write(struct.pack('<II',off,size))
    f.write(payload)
print(f'{a.output}: language={lang} id={lang_id} gender={a.gender} phrases={sum(1 for _,n in entries if n)} bytes={a.output.stat().st_size}')
