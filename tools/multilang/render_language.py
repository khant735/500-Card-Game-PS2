#!/usr/bin/env python3
import argparse, asyncio, json, os, re, struct, subprocess, time, wave
from pathlib import Path
import edge_tts
from deep_translator import GoogleTranslator
from voice_catalog import CLIPS

LANG_CFG = {
'HINDI': ('hi-IN','hi'), 'ARABIC': ('ar-EG','ar'), 'BENGALI': ('bn-BD','bn'),
'PORTUGUESE': ('pt-BR','pt'), 'INDONESIAN': ('id-ID','id'), 'URDU': ('ur-PK','ur'),
'RUSSIAN': ('ru-RU','ru'), 'CHINESE_MANDARIN': ('zh-CN','zh-CN'),
'CHINESE_CANTONESE': ('yue-HK','zh-TW'), 'SPANISH': ('es-ES','es'), 'FRENCH': ('fr-FR','fr'),
'GERMAN': ('de-DE','de'), 'ITALIAN': ('it-IT','it'), 'JAPANESE': ('ja-JP','ja'),
'KOREAN': ('ko-KR','ko'), 'DUTCH': ('nl-NL','nl'), 'THAI': ('th-TH','th'),
'FILIPINO_TAGALOG': ('fil-PH','tl'), 'VIETNAMESE': ('vi-VN','vi'), 'MALAY': ('ms-MY','ms'),
'TURKISH': ('tr-TR','tr'), 'PERSIAN': ('fa-IR','fa'), 'PUNJABI': ('pa-IN','pa'),
'TAMIL': ('ta-IN','ta'), 'SWAHILI': ('sw-KE','sw'), 'HAUSA': ('ha-NG','ha'),
'YORUBA': ('yo-NG','yo'), 'IGBO': ('ig-NG','ig'), 'AMHARIC': ('am-ET','am'),
'OROMO': ('om-ET','om'), 'SOMALI': ('so-SO','so'), 'ZULU': ('zu-ZA','zu'),
'XHOSA': ('xh-ZA','xh'), 'AFRIKAANS': ('af-ZA','af'), 'SHONA': ('sn-ZW','sn'),
'KINYARWANDA': ('rw-RW','rw'), 'MALAGASY': ('mg-MG','mg'), 'WOLOF': ('wo-SN','wo'),
'LINGALA': ('ln-CD','ln'), 'AKAN_TWI': ('ak-GH','ak'), 'FULA': ('ff-SN','ff'),
'TIGRINYA': ('ti-ET','ti'), 'SESOTHO': ('st-ZA','st'), 'SETSWANA': ('tn-ZA','tn'),
'CHICHEWA': ('ny-MW','ny'),
}
ALT_TARGETS = {
'CHINESE_MANDARIN':['zh-CN','zh'], 'CHINESE_CANTONESE':['zh-TW','zh'],
'FILIPINO_TAGALOG':['tl','fil'], 'AKAN_TWI':['ak'], 'FULA':['ff'],
}
RATE=os.environ.get('VOICE_RATE','+8%')
MAX_SECONDS=float(os.environ.get('MAX_SECONDS','6.15'))
CONCURRENCY=int(os.environ.get('TTS_CONCURRENCY','3'))
PHRASE_LIMIT=int(os.environ.get('PHRASE_LIMIT','0'))

def save_json(path,obj):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(obj,indent=2,ensure_ascii=False),encoding='utf-8')

_MARK_RE=re.compile(r'\\[\\[\\s*(\\d{3})\\s*\\]\\]')

def translate_all(texts,lang,targets):
    """Translate in marker-preserving chunks to stay below free endpoint limits."""
    last=''
    for target in targets:
        try:
            tr=GoogleTranslator(source='en',target=target)
        except Exception as e:
            last=f'{type(e).__name__}: {e}'
            continue
        result=[None]*len(texts); good=True
        for start in range(0,len(texts),12):
            stop=min(start+12,len(texts))
            joined='\\n'.join(f'[[{i:03d}]] {texts[i]}' for i in range(start,stop))
            translated=None
            for attempt in range(5):
                try:
                    translated=tr.translate(joined)
                    if translated: break
                except Exception as e:
                    last=f'{type(e).__name__}: {e}'
                time.sleep(2.0+attempt*2.0)
            if not translated:
                good=False; break
            marks=list(_MARK_RE.finditer(translated))
            if len(marks)!=(stop-start):
                last=f'batch markers changed by translator ({len(marks)} of {stop-start})'
                good=False; break
            seen=set()
            for j,m in enumerate(marks):
                idx=int(m.group(1))
                end=marks[j+1].start() if j+1<len(marks) else len(translated)
                seg=translated[m.end():end].strip()
                if idx<start or idx>=stop or not seg:
                    good=False; last='bad translated batch index/text'; break
                result[idx]=seg; seen.add(idx)
            if not good or len(seen)!=(stop-start):
                good=False; break
            print(f'translated {stop}/{len(texts)}',flush=True)
            time.sleep(0.8)
        if good and all(result):
            return result,target

    # Fallback: one request at a time, but deliberately throttled.
    for target in targets:
        try:
            tr=GoogleTranslator(source='en',target=target)
        except Exception as e:
            last=f'{type(e).__name__}: {e}'
            continue
        result=[]
        try:
            for text in texts:
                out=None
                for attempt in range(5):
                    try:
                        out=tr.translate(text)
                        if out: break
                    except Exception as e:
                        last=f'{type(e).__name__}: {e}'
                    time.sleep(2.0+attempt*2.0)
                if not out: raise RuntimeError(last or 'empty translation')
                result.append(out.strip())
                time.sleep(0.8)
            return result,target
        except Exception as e:
            last=f'{type(e).__name__}: {e}'
    raise RuntimeError(f'translation failed for {lang}: {last}')

def choose_voice(voices,locale,gender):
    want=gender.lower()
    c=[v for v in voices if str(v.get('Locale','')).lower()==locale.lower() and str(v.get('Gender','')).lower()==want]
    if not c:
        prefix=locale.split('-')[0].lower()
        c=[v for v in voices if str(v.get('Locale','')).lower().startswith(prefix+'-') and str(v.get('Gender','')).lower()==want]
    c.sort(key=lambda v:(0 if 'Neural' in v.get('ShortName','') else 1,v.get('ShortName','')))
    return c[0] if c else None

def ffmpeg_to_wav(src,dst):
    filt='silenceremove=start_periods=1:start_duration=0.03:start_threshold=-48dB:stop_periods=-1:stop_duration=0.08:stop_threshold=-48dB,dynaudnorm=f=150:g=9:p=0.9'
    subprocess.run(['ffmpeg','-loglevel','error','-y','-i',str(src),'-af',filt,'-ar','22050','-ac','1','-sample_fmt','s16',str(dst)],check=True)
    with wave.open(str(dst),'rb') as w:
        dur=w.getnframes()/w.getframerate()
    if dur>MAX_SECONDS:
        factor=dur/MAX_SECONDS
        tmp=dst.with_suffix('.short.wav')
        subprocess.run(['ffmpeg','-loglevel','error','-y','-i',str(dst),'-af',f'atempo={factor:.6f}','-ar','22050','-ac','1','-sample_fmt','s16',str(tmp)],check=True)
        tmp.replace(dst)

def clamp16(x):
    return -32768 if x < -32768 else 32767 if x > 32767 else x

_F0=(0,60,115,98,122); _F1=(0,0,-52,-55,-60)

def encode_block(src,h1,h2,flags=0):
    best=None
    for pred_id in range(5):
        s1,s2=h1,h2; peak=0
        for sample in src:
            pred=(s1*_F0[pred_id]+s2*_F1[pred_id]+32)>>6
            peak=max(peak,abs(sample-pred)); s2,s1=s1,sample
        shift=12
        while shift>0 and peak>(7<<(12-shift)): shift-=1
        for st in sorted(set((shift,min(shift+1,12)))):
            scale=1<<(12-st); rh1,rh2=h1,h2; err=0; qv=[]
            for sample in src:
                pred=(rh1*_F0[pred_id]+rh2*_F1[pred_id]+32)>>6
                rr=sample-pred
                q=(rr+scale//2)//scale if rr>=0 else -((-rr+scale//2)//scale)
                q=max(-8,min(7,q)); recon=clamp16(pred+q*scale)
                d=sample-recon; err+=d*d; qv.append(q); rh2,rh1=rh1,recon
            if best is None or err<best[0]: best=(err,pred_id,st,qv,rh1,rh2)
    _,pred_id,st,qv,nh1,nh2=best
    out=bytearray(16); out[0]=(pred_id<<4)|(st&15); out[1]=flags
    for i in range(14): out[2+i]=(qv[2*i]&15)|((qv[2*i+1]&15)<<4)
    return bytes(out),nh1,nh2

def encode_wav_adp(wav_path,out_path):
    with wave.open(str(wav_path),'rb') as w:
        if w.getnchannels()!=1 or w.getsampwidth()!=2: raise RuntimeError('WAV must be mono s16')
        rate=w.getframerate(); nframes=w.getnframes(); raw=w.readframes(nframes)
    samples=list(struct.unpack('<%dh'%(len(raw)//2),raw)) or [0]
    blocks=[]; h1=h2=0; nblocks=(len(samples)+27)//28
    for bi in range(nblocks):
        part=samples[bi*28:(bi+1)*28]; part += [0]*(28-len(part))
        block,h1,h2=encode_block(part,h1,h2,1 if bi==nblocks-1 else 0); blocks.append(block)
    end=bytearray(16); end[0]=blocks[-1][0]; end[1]=7; blocks.append(bytes(end))
    data=b''.join(blocks); pitch=int(rate*4096/48000)
    out_path.write_bytes(struct.pack('<4sIII',b'APCM',0x101,pitch,nframes)+data)
    if out_path.stat().st_size>131072: raise RuntimeError(f'ADP too large: {out_path.stat().st_size}')

async def synth_one(sem,text,voice,mp3):
    async with sem:
        last=None
        for attempt in range(5):
            try:
                await edge_tts.Communicate(text,voice,rate=RATE).save(str(mp3))
                if mp3.exists() and mp3.stat().st_size>256: return
                raise RuntimeError('empty audio')
            except Exception as e:
                last=e
                await asyncio.sleep(1.5*(attempt+1))
        raise last

async def synth_gender(translations,voice_name,gender_dir,work):
    sem=asyncio.Semaphore(CONCURRENCY)
    async def one(idx,name,text):
        mp3=work/f'{gender_dir.name}_{idx:03d}.mp3'; wav=work/f'{gender_dir.name}_{idx:03d}.wav'; adp=gender_dir/f'{name}.adp'
        if adp.exists(): return
        await synth_one(sem,text,voice_name,mp3)
        ffmpeg_to_wav(mp3,wav); encode_wav_adp(wav,adp)
        for p in (mp3,wav):
            try: p.unlink()
            except FileNotFoundError: pass
    for start in range(0,len(translations),18):
        results=await asyncio.gather(*[
            asyncio.create_task(one(i,name,translations[i]))
            for i,(name,_) in enumerate(CLIPS[start:start+18],start)
        ],return_exceptions=True)
        errs=[e for e in results if isinstance(e,Exception)]
        if errs: raise errs[0]
        print(f'{gender_dir.name}: {min(start+18,len(translations))}/{len(translations)}',flush=True)

async def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--language',required=True)
    ap.add_argument('--output',type=Path,default=Path('out'))
    a=ap.parse_args()
    lang=a.language.upper(); out=a.output/lang; out.mkdir(parents=True,exist_ok=True)
    active_clips=CLIPS[:PHRASE_LIMIT] if PHRASE_LIMIT>0 else CLIPS
    report={'language':lang,'status':'starting','phrase_count':len(active_clips),'catalogue_count':len(CLIPS),'rate':RATE,'max_seconds':MAX_SECONDS}
    if lang not in LANG_CFG:
        report.update(status='unsupported',reason='not in renderer configuration'); save_json(out/'REPORT.json',report); return 0
    locale,target=LANG_CFG[lang]
    voices=await edge_tts.list_voices()
    male=choose_voice(voices,locale,'Male'); female=choose_voice(voices,locale,'Female')
    report['locale']=locale; report['voices']={'male':male['ShortName'] if male else None,'female':female['ShortName'] if female else None}
    if not male and not female:
        report.update(status='unsupported',reason=f'no Edge neural voice found for locale {locale}'); save_json(out/'REPORT.json',report); return 0
    if not male or not female:
        report.update(status='partial',reason='only one native neural gender available; no pitch-shifted fake gender emitted')
        save_json(out/'REPORT.json',report); return 0

    translations=[]; used=[]
    targets=ALT_TARGETS.get(lang,[target])
    cache=out/'translations.tsv'
    if cache.exists():
        rows=cache.read_text(encoding='utf-8').splitlines()[1:]
        if len(rows)==len(active_clips): translations=[r.split('\t',2)[2] for r in rows]
    if not translations:
        try:
            translations,t=translate_all([en for _,en in active_clips],lang,targets)
            used.append(t)
        except Exception as e:
            report.update(status='unsupported',reason=str(e))
            save_json(out/'REPORT.json',report); return 0
        with cache.open('w',encoding='utf-8') as f:
            f.write('id\\tname\\ttranslation\\n')
            for i,((name,_),tr) in enumerate(zip(active_clips,translations)):
                f.write(f'{i}\\t{name}\\t{tr.replace(chr(9)," ")}\\n')

    work=out/'_work'; work.mkdir(exist_ok=True)
    male_dir=out/'MALE_ADP'; female_dir=out/'FEMALE_ADP'; male_dir.mkdir(exist_ok=True); female_dir.mkdir(exist_ok=True)
    try:
        await synth_gender(translations,male['ShortName'],male_dir,work)
        await synth_gender(translations,female['ShortName'],female_dir,work)
    except Exception as e:
        report.update(status='failed',reason=f'{type(e).__name__}: {e}')
        save_json(out/'REPORT.json',report); return 1
    report.update(status='rendered',male_voice=male['ShortName'],female_voice=female['ShortName'],translation_targets=sorted(set(used)))
    save_json(out/'REPORT.json',report)
    return 0

if __name__=='__main__':
    raise SystemExit(asyncio.run(main()))
