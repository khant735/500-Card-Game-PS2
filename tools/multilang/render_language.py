#!/usr/bin/env python3
import argparse, asyncio, json, os, subprocess, time, wave
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

def save_json(path,obj):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(obj,indent=2,ensure_ascii=False),encoding='utf-8')

def translate_one(text,lang,targets):
    last=''
    for target in targets:
        for attempt in range(4):
            try:
                out=GoogleTranslator(source='en',target=target).translate(text)
                if out and out.strip() and out.strip().lower()!=text.strip().lower():
                    return out.strip(),target
                last='translation unchanged/empty'
            except Exception as e:
                last=f'{type(e).__name__}: {e}'
            time.sleep(1.0+attempt*1.5)
    raise RuntimeError(f'translation failed for {lang}: {last}')

def choose_voice(voices,locale,gender):
    want=gender.lower()
    c=[v for v in voices if str(v.get('Locale','')).lower()==locale.lower() and str(v.get('Gender','')).lower()==want]
    if not c:
        prefix=locale.split('-')[0].lower()
        c=[v for v in voices if str(v.get('Locale','')).lower().startswith(prefix+'-') and str(v.get('Gender','')).lower()==want]
    c.sort(key=lambda v:(0 if 'Neural' in v.get('ShortName','') else 1,v.get('ShortName','')))
    return c[0] if c else None

def to_wav(src,dst):
    filt='silenceremove=start_periods=1:start_duration=0.03:start_threshold=-48dB:stop_periods=-1:stop_duration=0.08:stop_threshold=-48dB,dynaudnorm=f=150:g=9:p=0.9'
    subprocess.run(['ffmpeg','-loglevel','error','-y','-i',str(src),'-af',filt,'-ar','22050','-ac','1','-sample_fmt','s16',str(dst)],check=True)
    with wave.open(str(dst),'rb') as w:
        dur=w.getnframes()/w.getframerate()
    if dur>MAX_SECONDS:
        factor=dur/MAX_SECONDS
        tmp=dst.with_suffix('.short.wav')
        subprocess.run(['ffmpeg','-loglevel','error','-y','-i',str(dst),'-af',f'atempo={factor:.6f}','-ar','22050','-ac','1','-sample_fmt','s16',str(tmp)],check=True)
        tmp.replace(dst)

def encode_adp(adpenc,wav,out):
    cp=subprocess.run([str(adpenc),str(wav),str(out)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    if cp.returncode!=0 or not out.exists():
        raise RuntimeError(f'adpenc failed ({cp.returncode}): {cp.stdout[-1200:]}')
    b=out.read_bytes()
    if len(b)<32 or b[:4]!=b'APCM': raise RuntimeError(f'invalid ADP: {out}')
    if len(b)>131072: raise RuntimeError(f'ADP too large: {len(b)}')

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

async def synth_gender(translations,voice_name,gender_dir,work,adpenc):
    sem=asyncio.Semaphore(CONCURRENCY)
    async def one(idx,name,text):
        mp3=work/f'{gender_dir.name}_{idx:03d}.mp3'
        wav=work/f'{gender_dir.name}_{idx:03d}.wav'
        adp=gender_dir/f'{name}.adp'
        if adp.exists(): return
        await synth_one(sem,text,voice_name,mp3)
        to_wav(mp3,wav); encode_adp(adpenc,wav,adp)
        for p in (mp3,wav):
            try: p.unlink()
            except FileNotFoundError: pass
    for start in range(0,len(CLIPS),18):
        results=await asyncio.gather(*[
            asyncio.create_task(one(i,name,translations[i]))
            for i,(name,_) in enumerate(CLIPS[start:start+18],start)
        ],return_exceptions=True)
        errs=[e for e in results if isinstance(e,Exception)]
        if errs: raise errs[0]
        print(f'{gender_dir.name}: {min(start+18,len(CLIPS))}/{len(CLIPS)}',flush=True)

async def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--language',required=True)
    ap.add_argument('--output',type=Path,default=Path('out'))
    ap.add_argument('--adpenc',type=Path,required=True)
    a=ap.parse_args()
    lang=a.language.upper(); out=a.output/lang; out.mkdir(parents=True,exist_ok=True)
    report={'language':lang,'status':'starting','phrase_count':len(CLIPS),'rate':RATE,'max_seconds':MAX_SECONDS}
    if lang not in LANG_CFG:
        report.update(status='unsupported',reason='not in renderer configuration'); save_json(out/'REPORT.json',report); return 0
    locale,target=LANG_CFG[lang]
    voices=await edge_tts.list_voices()
    male=choose_voice(voices,locale,'Male'); female=choose_voice(voices,locale,'Female')
    report['locale']=locale
    report['voices']={'male':male['ShortName'] if male else None,'female':female['ShortName'] if female else None}
    if not male and not female:
        report.update(status='unsupported',reason=f'no Edge neural voice found for {locale}'); save_json(out/'REPORT.json',report); return 0
    if not male or not female:
        report.update(status='partial',reason='only one native neural gender available; no pitch-shifted fake gender emitted')
        save_json(out/'REPORT.json',report); return 0

    targets=ALT_TARGETS.get(lang,[target]); translations=[]; used=[]
    cache=out/'translations.tsv'
    if cache.exists():
        rows=cache.read_text(encoding='utf-8').splitlines()[1:]
        if len(rows)==len(CLIPS): translations=[r.split('\t',2)[2] for r in rows]
    if not translations:
        for i,(name,en) in enumerate(CLIPS):
            try: tr,t=translate_one(en,lang,targets)
            except Exception as e:
                report.update(status='unsupported',reason=str(e),translation_index=i,translation_name=name)
                save_json(out/'REPORT.json',report); return 0
            translations.append(tr); used.append(t)
            if (i+1)%20==0: print(f'translated {i+1}/{len(CLIPS)}',flush=True)
        with cache.open('w',encoding='utf-8') as f:
            f.write('id\tname\ttranslation\n')
            for i,((name,_),tr) in enumerate(zip(CLIPS,translations)):
                f.write(f'{i}\t{name}\t{tr.replace(chr(9)," ")}\n')

    work=out/'_work'; work.mkdir(exist_ok=True)
    male_dir=out/'MALE_ADP'; female_dir=out/'FEMALE_ADP'
    male_dir.mkdir(exist_ok=True); female_dir.mkdir(exist_ok=True)
    try:
        await synth_gender(translations,male['ShortName'],male_dir,work,a.adpenc)
        await synth_gender(translations,female['ShortName'],female_dir,work,a.adpenc)
    except Exception as e:
        report.update(status='failed',reason=f'{type(e).__name__}: {e}')
        save_json(out/'REPORT.json',report); return 1
    report.update(status='rendered',male_voice=male['ShortName'],female_voice=female['ShortName'],translation_targets=sorted(set(used)))
    save_json(out/'REPORT.json',report)
    return 0

if __name__=='__main__':
    raise SystemExit(asyncio.run(main()))
