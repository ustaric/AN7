import os
import re
from pathlib import Path

ROOT_DIR = Path(".").resolve()
OUTPUT_FILE = "CPP_프로젝트안내.md"

# C++ 소스 및 윈도우 11 쉘 등록 매니페스트 포함
TARGET_EXTENSIONS = {".cpp", ".h", ".hpp", ".idl", ".appxmanifest"}

# 순수 압축/암호화 백엔드 엔진 폴더만 정밀 제외 (UI/쉘/코어는 유지)
CODEC_ENGINES = {
    "zstandard", "brotli", "kanzi", "lizard", "lz4", "lz5", "xxhash", 
    "rhash", "freebsd", "littlefs", "gmssl", "zstdmt", "blake3"
}
IGNORE_DIRS = {
    ".git", ".vs", ".vscode", "bin", "obj", "x64", "x86", "arm64", 
    "debug", "release", "packages", "__pycache__", "out", "build"
} | CODEC_ENGINES

IGNORED_INCLUDES = {
    "iostream", "vector", "string", "memory", "algorithm", "windows.h", 
    "stdio.h", "stdlib.h", "tchar.h", "stdafx.h", "string.h"
}

def clean_comments(text, ext):
    if ext in {".cpp", ".h", ".hpp", ".idl"}:
        return re.sub(r"//.*?$|/\*.*?\*/", "", text, flags=re.MULTILINE | re.DOTALL)
    elif ext == ".appxmanifest":
        return re.sub(r"<!--.*?-->", "", text, flags=re.DOTALL)
    return text

def extract_summary(raw_text):
    """클래스/인터페이스 설명 주석 추출"""
    match = re.search(r"///\s*([^\n\r]+)", raw_text)
    if match:
        return match.group(1).strip()[:140]
    match_block = re.search(r"/\*\*\s*([\s\S]*?)\*/", raw_text)
    if match_block:
        cleaned = " ".join([l.strip().lstrip("* ") for l in match_block.group(1).split("\n") if l.strip()])
        return cleaned[:140]
    return None

def analyze_tags(clean_text, filename):
    tags = []
    if any(k in clean_text for k in ("IExplorerCommand", "IEnumExplorerCommand")):
        tags.append("Win11-ModernShell")
    if any(k in clean_text for k in ("IContextMenu", "IShellExtInit")):
        tags.append("Win10-ClassicShell")
    if "winrt::" in clean_text or "C++/WinRT" in clean_text:
        tags.append("C++/WinRT")
    if any(k in clean_text for k in ("ExtractArchives", "ArchiveExtractCallback", "kExtractHere")):
        tags.append("Extract-Engine")
    if any(k in clean_text for k in ("CreateProcess", "ShellExecute", "MyCreateProcess")):
        tags.append("Process-Execution")
    if "windows.fileExplorerContextMenu" in clean_text:
        tags.append("Shell-Manifest")
    return tags

def extract_classes_and_inheritance(clean_text):
    """클래스명과 상속 인터페이스 추출 (예: ExplorerCommandRoot : public IExplorerCommand)"""
    matches = re.findall(r"\bclass\s+([A-Za-z_]\w*)(?:\s*:\s*(?:public|protected|private)?\s*([A-Za-z_]\w*))?", clean_text)
    results = []
    for cls, base in matches:
        if base:
            results.append(f"{cls} (: {base})")
        else:
            results.append(cls)
    return results[:4]

def extract_guids(raw_text):
    """선언된 CLSID / GUID 추출"""
    guids = re.findall(r'(?:DECLSPEC_UUID\("([A-Fa-f0-9\-]+)"\)|DEFINE_GUID\(\w+,\s*(0x[0-9a-fA-F]+[^\)]+)\))', raw_text)
    flat = []
    for g1, g2 in guids:
        if g1: flat.append(g1)
        elif g2: flat.append("GUID-Def")
    return flat[:2]

def extract_methods(clean_text):
    """비즈니스 및 쉘 핵심 메서드 선별"""
    raw_methods = re.findall(
        r"\b(?:HRESULT|BOOL|void|int|DWORD|UString)?\s*(?:[A-Za-z_]\w*::)?([A-Za-z_]\w*)\s*\([^;{}()]*\)\s*(?:const)?\s*(?:override)?\s*[{;]",
        clean_text
    )
    ignore = {"if", "for", "while", "switch", "return", "sizeof", "AddRef", "Release", "QueryInterface"}
    filtered = [m for m in set(raw_methods) if m not in ignore and len(m) > 2]

    def score(m):
        val = 0
        if any(k in m for k in ("Invoke", "GetTitle", "GetState", "GetIcon", "EnumSubCommands")): val += 15
        if any(k in m for k in ("QueryContextMenu", "Initialize", "Extract", "Open")): val += 12
        if any(k in m for k in ("Command", "Process", "Execute", "Compress")): val += 8
        return val

    filtered.sort(key=lambda x: (-score(x), x))
    return filtered[:6]

def main():
    print("[*] 핵심 아키텍처 스캔 중 (순수 알고리즘 엔진 제외)...")
    file_list = []
    analyzed = []
    shell_pipeline = []

    for root, dirs, files in os.walk(ROOT_DIR):
        dirs[:] = [d for d in dirs if d.lower() not in IGNORE_DIRS]
        for f in files:
            p = Path(root) / f
            ext = p.suffix.lower()
            if ext in TARGET_EXTENSIONS:
                try:
                    with open(p, "r", encoding="utf-8", errors="ignore") as fp:
                        raw = fp.read()
                        clean = clean_comments(raw, ext)
                except Exception:
                    continue

                lines = raw.count("\n") + 1
                size_kb = round(p.stat().st_size / 1024, 1)
                tags = analyze_tags(clean, f)
                classes = extract_classes_and_inheritance(clean)
                guids = extract_guids(raw)
                methods = extract_methods(clean) if ext != ".appxmanifest" else []

                includes = [
                    inc for inc in re.findall(r'#include\s*[<"]([^>"]+)[>"]', clean)
                    if inc.lower() not in IGNORED_INCLUDES
                ][:4]

                item = {
                    "path": p.relative_to(ROOT_DIR),
                    "name": f,
                    "ext": ext,
                    "lines": lines,
                    "size_kb": size_kb,
                    "summary": extract_summary(raw),
                    "tags": tags,
                    "classes": classes,
                    "guids": guids,
                    "methods": methods,
                    "includes": includes
                }
                analyzed.append(item)

                if any(t in tags for t in ("Win11-ModernShell", "Win10-ClassicShell", "Extract-Engine", "Shell-Manifest")):
                    shell_pipeline.append(item)

    print("[*] 마크다운 가이드 문서 작성 중...")
    md = [
        "# ⚡ NanaZip 핵심 아키텍처 & 쉘 확장 사양서",
        f"> **분석된 핵심 모듈 파일**: 총 {len(analyzed)}개 | **목적**: 우클릭 컨텍스트 메뉴 커스텀 가이드",
        "",
        "## 🎯 1. 쉘 확장 및 압축해제 파이프라인 매핑",
        "우클릭 메뉴('여기에 풀기' 등)를 제어할 때 직접적으로 연결되는 주요 파일 체인입니다.",
        "",
        "| 구분 / 레이어 | 핵심 파일 | 주요 역할 및 상속/구현 |",
        "| :--- | :--- | :--- |"
    ]

    for item in sorted(shell_pipeline, key=lambda x: str(x["path"])):
        tag_str = ", ".join([f"`{t}`" for t in item["tags"]]) if item["tags"] else "`Core`"
        cls_str = ", ".join([f"`{c}`" for c in item["classes"]]) if item["classes"] else "-"
        md.append(f"| {tag_str} | `{item['name']}` | {cls_str} |")

    md.extend([
        "",
        "---",
        "## 📁 2. 주요 쉘 & UI 핵심 소스 상세 분석",
        "> *단순 인라인 헤더 및 코덱 엔진을 제외한 실제 동작 파일 명세입니다.*",
        ""
    ])

    # 40라인 이상이면서 태그가 있거나 주요 경로에 있는 파일만 선정
    detail_targets = [
        f for f in analyzed 
        if (f["lines"] >= 40 and f["tags"]) or "ShellExtension" in f["name"] or "ContextMenu" in f["name"]
    ]
    detail_targets.sort(key=lambda x: str(x["path"]))

    for f in detail_targets:
        md.append(f"### 📄 `{f['path']}` ({f['lines']} lines, {f['size_kb']} KB)")
        if f["tags"]:
            md.append(f"- **핵심 태그**: {' '.join([f'`[{t}]`' for t in f['tags']])}")
        if f["guids"]:
            md.append(f"- **할당 GUID/CLSID**: {', '.join([f'`{g}`' for g in f['guids']])}")
        if f["classes"]:
            md.append(f"- **정의 클래스**: {', '.join([f'`{c}`' for c in f['classes']])}")
        if f["methods"]:
            md.append(f"- **주요 실행 함수**: `{', '.join(f['methods'])}`")
        if f["includes"]:
            md.append(f"- **연관 헤더**: `{', '.join(f['includes'])}`")
        if f["summary"]:
            md.append(f"- **개요**: {f['summary']}")
        md.append("")

    with open(OUTPUT_FILE, "w", encoding="utf-8") as out:
        out.write("\n".join(md))

    out_size = round(Path(OUTPUT_FILE).stat().st_size / 1024, 1)
    print(f"[+] 완료: '{OUTPUT_FILE}' 생성됨 (밸런스 조정 크기: {out_size} KB)")

if __name__ == "__main__":
    main()