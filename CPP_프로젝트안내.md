# ⚡ NanaZip 핵심 아키텍처 & 쉘 확장 사양서
> **분석된 핵심 모듈 파일**: 총 5개 | **목적**: 우클릭 컨텍스트 메뉴 커스텀 가이드

## 🎯 1. 쉘 확장 및 압축해제 파이프라인 매핑
우클릭 메뉴('여기에 풀기' 등)를 제어할 때 직접적으로 연결되는 주요 파일 체인입니다.

| 구분 / 레이어 | 핵심 파일 | 주요 역할 및 상속/구현 |
| :--- | :--- | :--- |

---
## 📁 2. 주요 쉘 & UI 핵심 소스 상세 분석
> *단순 인라인 헤더 및 코덱 엔진을 제외한 실제 동작 파일 명세입니다.*

### 📄 `SmartArchiver C++.cpp` (531 lines, 20.6 KB)
- **핵심 태그**: `[Process-Execution]`
- **주요 실행 함수**: `ExtractHere, SmartExtract, GetExitCodeProcess, SmartCompress, AddArchiveRootItem, CloseHandle`
- **연관 헤더**: `shlobj.h, map, sstream`
