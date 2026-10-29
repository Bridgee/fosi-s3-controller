"""Embed the local source icon/identity registry in both offline previews."""
from pathlib import Path
import json
import re

root = Path(__file__).resolve().parents[1]
icons = root / "assets/source-icons"
protocols = json.loads((icons / "protocols.json").read_text())
entries = {alias: {"label": item["label"], "icon": item["icon"]}
           for item in protocols for alias in item["aliases"]}
registry = "// orbit-source-registry:start\nconst networkSources=" + json.dumps(entries, separators=(",", ":")) + r""";
function networkSource(value){const key=String(value??'').toLowerCase().replace(/[\s_-]+/g,'');return Object.prototype.hasOwnProperty.call(networkSources,key)?networkSources[key]:{label:'Network',icon:'network'};}
function currentSource(){if(state.input==='network')return state.networkActive?networkSource(state.networkProtocol):{label:'Network',icon:'network'};return {label:shortLabels[state.input]||'Unknown source',icon:{bluetooth:'bluetooth',line:'line',hdmi:'hdmi',optical:'optical'}[state.input]||null};}
function renderSourceIcons(identity){for(const id of ['sourceStatusIcon','routeSourceIcon']){const use=$(id);use.parentElement.style.display=identity.icon?'':'none';if(identity.icon)use.setAttribute('href','#orbit-src-'+identity.icon);else use.removeAttribute('href');}}
// orbit-source-registry:end"""
symbols = (icons / "symbols.html").read_text()
css = """
.ui-icon{display:block;flex-shrink:0;aspect-ratio:1 / 1;overflow:visible}
.status{gap:8px;letter-spacing:1.2px}
.status>.ui-icon{width:20px;height:20px;min-width:20px;max-width:20px;flex:0 0 20px;stroke:none;color:var(--accent,#9bcead)}
.status>strong{min-width:0;max-width:250px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;line-height:14px}
.s3-current-source>.ui-icon{width:20px;height:20px;min-width:20px;max-width:20px;flex:0 0 20px;stroke:none}
.s3-choice[data-output="analog"]>svg{width:26px!important;height:26px!important;min-width:26px;max-width:26px;flex:0 0 26px;margin:0}
"""
for name in ["index.html", "orbit-s3-preview.html"]:
    target = root / name
    source = target.read_text()
    source = re.sub(r'<symbol id="orbit-src-[^"]+"[\s\S]*?</symbol>\s*', '', source)
    if name == "index.html":
        source = source.replace('</defs></svg>', symbols + '</defs></svg>', 1)
    else:
        if '<svg width="0"' not in source:
            source = source.replace('<div id="orbit-s3-preview" class="viz-dotted-background">',
                '<div id="orbit-s3-preview" class="viz-dotted-background">\n<svg width="0" height="0" style="position:absolute;overflow:hidden" aria-hidden="true"><defs></defs></svg>', 1)
        source = source.replace('</defs></svg>', symbols + '</defs></svg>', 1)
        for identifier in ['sourceStatusIcon', 'routeSourceIcon']:
            source = re.sub(r'<i id="' + identifier + r'"[^>]*></i>',
                '<svg><use id="' + identifier + '" href="#orbit-src-spotify"/></svg>', source)
    scoped = css if name == "index.html" else '\n'.join(
        '#orbit-s3-preview ' + line if line.startswith('.') else line for line in css.splitlines())
    source = re.sub(r'/\* orbit-source-layout:start \*/[\s\S]*?/\* orbit-source-layout:end \*/', '', source)
    source = source.replace('</style>', '/* orbit-source-layout:start */' + scoped + '\n/* orbit-source-layout:end */\n</style>', 1)
    def svg_box(match):
        attrs = match.group(1)
        if 'viewBox=' not in attrs: attrs += ' viewBox="0 0 24 24"'
        if 'preserveAspectRatio=' not in attrs: attrs += ' preserveAspectRatio="xMidYMid meet"'
        if 'class=' not in attrs: attrs += ' class="ui-icon"'
        if 'aria-hidden=' not in attrs: attrs += ' aria-hidden="true"'
        return '<svg' + attrs + '><use'
    source = re.sub(r'<svg([^>]*)><use', svg_box, source)
    for identifier in ['sourceStatusIcon', 'routeSourceIcon']:
        source = re.sub(r'<use id="' + identifier + r'" href="[^"]+"',
            '<use id="' + identifier + '" href="#orbit-src-spotify"', source)
    if '// orbit-source-registry:start' in source:
        source = re.sub(r'// orbit-source-registry:start[\s\S]*?// orbit-source-registry:end', lambda _: registry, source, count=1)
    else:
        source = re.sub(r'const networkLabels=\{[^\n]+\};', lambda _: registry, source, count=1)
    source=source.replace("for(const id of ['sourceStatusIcon','routeSourceIcon'])$(id).setAttribute('href','#orbit-src-'+reportedSource.icon);", "renderSourceIcons(reportedSource);")
    assert "renderSourceIcons(reportedSource);" in source,name
    # Preserve render's power, source-status and other current UI state handling.
    assert "const reportedSource=currentSource();" in source, name
    source = re.sub(r"\$\('sourceName'\)\.textContent=[^;]+;\$\('routeSource'\)\.textContent=[^;]+;",
        "$('sourceName').textContent=reportedSource.label.toUpperCase();$('routeSource').textContent=reportedSource.label;", source, count=1)
    source = source.replace("if(Object.hasOwn(networkLabels,previewParams.get('protocol')))state.networkProtocol=previewParams.get('protocol');",
        "if(previewParams.has('protocol'))state.networkProtocol=previewParams.get('protocol').slice(0,63);")
    # Routing choices share the same connector assets as the current-source badge.
    # Keep Bluetooth unchanged; share physical input/output connector assets.
    for attribute, key in [('data-input="line"','line'), ('data-input="hdmi"','hdmi'),
                           ('data-input="optical"','optical'), ('data-output="optical"','optical'), ('data-output="analog"','analog')]:
        svg = '<svg viewBox="0 0 24 24" preserveAspectRatio="xMidYMid meet" class="ui-icon" aria-hidden="true"><use href="#orbit-src-' + key + '"/></svg>'
        source = re.sub(r'(<button[^>]*' + attribute + r'[^>]*>)(?:<svg[\s\S]*?</svg>|<i[^>]*></i>)',
                        lambda match: match[1] + svg, source, count=1)
    target.write_text(source)
print("Both previews now embed the same source registry and square, aspect-preserving icons.")
