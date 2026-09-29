"""Analyze actual plugin event timestamps; never substitute simulated FPS."""
import argparse, csv, json, statistics
from pathlib import Path

def summarize(path,skip,expected):
    with path.open(encoding='utf-8-sig',newline='') as source:
        rows=list(csv.DictReader(source))
    if not rows: raise ValueError('Empty trace')
    start=float(rows[0]['qpc_seconds'])+skip
    groups={}
    for row in rows:
        if float(row['qpc_seconds'])>=start:
            groups.setdefault(row['event'],[]).append(row)
    result={'file':str(path),'skipped_seconds':skip,'events':{}}
    for name,items in groups.items():
        times=[float(row['qpc_seconds']) for row in items]
        gaps=[b-a for a,b in zip(times,times[1:])]
        duration=times[-1]-times[0]
        result['events'][name]={'count':len(items),'rate_hz':(len(items)-1)/duration if duration>0 else None,
          'median_gap_ms':statistics.median(gaps)*1000 if gaps else None,
          'max_gap_ms':max(gaps)*1000 if gaps else None}
    commands=groups.get('usercmd',[])
    if commands:
        durations=[float(row['frame_time']) for row in commands]
        seq=[int(row['sequence']) for row in commands]
        result['commands']={'average_frame_ms':statistics.mean(durations)*1000,
          'max_frame_ms':max(durations)*1000,'below_5ms':sum(d<0.005-1e-7 for d in durations),
          'nonconsecutive_sequences':sum(b!=a+1 for a,b in zip(seq,seq[1:]))}
    else: result['warning']='No actual C_Player::CreateMove commands recorded; no transition conclusion possible'
    result['fallback_events']=len(groups.get('fallback',[]))
    if expected and groups.get('render_begin'):
        measured=result['events']['render_begin']['rate_hz']
        result['expected_render_fps']=expected
        result['render_rate_matches']=bool(measured and abs(measured-expected)<=max(2,expected*.03))
    result['measurement']='QPC timestamps: BeginFrame submissions and real C_Player::CreateMove calls; not scanout or input-to-photon latency'
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('trace',type=Path)
    p.add_argument('--skip-seconds',type=float,default=0)
    p.add_argument('--expected-render-fps',type=float)
    p.add_argument('--output',type=Path)
    a=p.parse_args()
    result=summarize(a.trace,a.skip_seconds,a.expected_render_fps)
    text=json.dumps(result,indent=2,ensure_ascii=False)
    if a.output: a.output.write_text(text+'\n',encoding='utf-8')
    print(text)

if __name__=='__main__': main()
