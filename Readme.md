# SmartArchiver / AN7 (C++ Win32)

> **[⚠️ Beta / Test Version]**  
> **Windows 탐색기 우클릭 메뉴 연동 및 스마트 압축 해제 / 새 폴더 관리 유틸리티**  
> 별도의 상주 프로세스나 외부 런타임 없이, 순수 C++ Win32 API로 작성된 단일 실행 파일(`.exe`)과 윈도우 레지스트리를 통해 연동되도록 구현되었습니다.

---

## ⚠️ 테스트 버전 및 면책 공지 (Notice & Disclaimer)

> **본 프로그램은 현재 기능 검증 및 테스트 단계인 [베타(Beta) 버전]입니다.**  
> 본 프로젝트는 **AI(인공지능) 코딩 어시스턴트의 지원을 받아 개발**되었으며, 윈도우 레지스트리 등록 및 파일 시스템 조작을 수반하므로 아래 면책 조항을 반드시 숙지한 후 사용하시기 바랍니다.

- **비파괴 안전 설계**: 본 유틸리티는 소스 코드 내에 파일/폴더 삭제 API(`DeleteFile`, `RemoveDirectory` 등)가 일체 포함되어 있지 않으며, 파일 덮어쓰기 등의 실제 압축 해제 작업은 7-Zip / NanaZip 공식 GUI 엔진의 안전 대화상자에 전적으로 위임하여 동작합니다.

- **AI 지원 개발 (AI-Assisted)**: 본 프로젝트는 최신 C++17 규격 및 Win32 API 표준에 맞추어 AI 코딩 어시스턴트의 지원을 받아 개발 및 검증되었습니다.

- **면책 조항 (Disclaimer)**: 본 프로그램은 개인 및 기업 모두에게 무료로 제공되는 오픈소스/베타 소프트웨어(AS-IS)입니다. 레지스트리 등록 및 외부 압축 엔진과의 연동 과정에서 발생할 수 있는 시스템 환경 차이나 사용자의 취급 부주의로 인한 문제에 대해 개발자는 법적 책임을 지지 않으므로, 중요한 환경에서는 테스트 후 사용하시기 바랍니다.
---

## 1. 아키텍처 및 레지스트리 어댑터 구조

SmartArchiver는 백그라운드 서비스나 쉘 익스텐션 DLL(In-Process COM Server)을 사용하지 않고, 레지스트리에 명령줄을 등록하여 호출되는 **어댑터(Adapter) 방식**으로 구현되었습니다.

### 1-1. 실행 파이프라인

    [사용자 파일/배경 우클릭]
          │
          ▼
    [Windows 탐색기 (Explorer)]
          │ (HKCU 레지스트리에 등록된 명령줄 인수로 실행)
          ▼
    [SmartArchiver (AN7.exe)] ◀─── 【 어댑터 계층 】
          │ 1. 자식 프로세스 파이프를 통해 압축 엔진 콘솔(7z.exe / K7C.exe) 실행 (`l -slt`)
          │ 2. 압축 파일 내부의 루트 디렉터리 구조 및 파일 목록 분석
          │ 3. 단일 루트 폴더 여부에 따라 압축 해제 대상 경로 계산 및 디렉터리 준비
          ▼
    [NanaZipG.exe / 7zG.exe]  ◀─── 【 GUI 압축 해제 】
          (계산된 경로 인수를 전달받아 공식 압축 해제 창 표시)

### 1-2. HKCU 레지스트리 등록 방식의 특징

    - 프로세스 비상주 설계: 평상시 백그라운드에서 상주하지 않으며, 사용자가 우클릭 메뉴를 선택했을 때만 프로세스가 기동되어 작업을 처리한 뒤 즉시 종료되도록 구현되었습니다.
    - 현재 사용자(HKCU) 범위 등록: HKEY_LOCAL_MACHINE이 아닌 HKEY_CURRENT_USER\Software\Classes 경로를 사용하므로, 관리자 권한 팝업 없이 등록 및 수정이 가능합니다.
    - 시스템 종속성 최소화: 탐색기 프로세스 내부에 DLL을 로드하는 인젝션 방식을 사용하지 않아 시스템 안정성에 미치는 영향을 최소화했습니다.
    - 간편한 등록 해제: 설정 창의 [전체 해제] 기능을 통해 등록된 레지스트리 키(`SmartExtract_*`, `SmartNewFolder`)를 일괄 제거할 수 있도록 구현되었습니다.

### 1-3. 내장 리소스 아이콘 매핑

    - 실행 파일에 포함된 리소스 번호를 탐색기 레지스트리의 Icon 항목에 지정하도록 처리되었습니다.
    - NanaZip  : 리소스 ID 131번 (nana.ico) ➔ "경로\AN7.exe",-131
    - 7-Zip    : 리소스 ID 130번 (7z.ico)   ➔ "경로\AN7.exe",-130
    - 새 폴더   : shell32.dll 기본 아이콘 인덱스 연결

---

## 2. 주요 기능

1. **설치된 7-Zip 계열 압축 엔진 자동 탐색**
   - **NanaZip** (App Paths 레지스트리 및 WindowsApps 환경 변수 경로 확인)
   - **7-Zip** (기본 설치 경로 `Program Files` 및 App Paths 확인)
   - **7-Zip-Zstandard** (ZS 빌드 실행 파일 호환)
   - 시스템에 설치된 엔진을 탐색하여 설정 창 목록에 표시하며, 원하는 엔진을 선택해 등록할 수 있습니다.

2. **압축 구조 기반 "알아서 풀기" (Smart Extract)**
   - 압축 엔진의 목록 조회(`l -slt`) 출력을 분석하여 해제 위치를 자동으로 분기합니다.
     - **단일 폴더 구조**: 압축 파일 내 최상위 항목이 1개의 폴더인 경우, 현재 디렉터리에 바로 해제하도록 경로 전달
     - **다중 파일/루트 분산 구조**: 파일들이 루트에 풀려 섞이지 않도록 압축 파일명으로 폴더를 생성한 후 해제하도록 경로 전달
   - 실제 해제 과정은 각 압축 프로그램의 공식 GUI 창을 통해 진행 상황이 표시됩니다.

3. **배경 메뉴 "새 폴더" 생성 기능**
   - 폴더 및 바탕화면 빈 공간 우클릭 메뉴에 등록할 수 있도록 구현되었습니다.
   - **입력 팝업 모드**:
     - 시스템 테마(`AppsUseLightTheme`)를 확인하여 다크 모드 및 라이트 모드에 맞추어 배경과 폰트 색상을 구성한 Win32 대화상자를 띄웁니다.
     - 기본 이름이 선택된 상태로 포커스되며, `Enter` 입력 시 생성, `Esc` 입력 시 취소됩니다.
   - **즉시 생성 모드**: 팝업 없이 고유 폴더명을 계산하여 바로 디렉터리를 생성합니다.
   - **날짜 포함 옵션**: `새 폴더 (YYYY-MM-DD)` 형태의 이름 포맷을 지원합니다.
   - **중복 이름 방지**: 대상 경로에 동일한 폴더명이 존재할 경우 `(1)`, `(2)` 형태의 순번을 부여하여 충돌을 방지합니다.

---

## 3. 다국어 지원 (`lang.json`)

코드 재컴파일 없이 텍스트를 변경할 수 있도록 외부 JSON 기반의 언어 매핑을 지원합니다.

### 3-1. 동작 방식
1. 프로그램 실행 시 실행 파일과 동일한 디렉터리의 `lang.json` 파일을 확인합니다.
2. `lang.json` 파일이 존재하지 않는 경우 프로그램 내부의 기본 한국어 문자열로 동작합니다.
3. 파일이 존재하는 경우 해당 키값을 읽어와 UI 텍스트 및 레지스트리 등록 메뉴명을 대체합니다.

### 3-2. 언어 설정 키 목록

| JSON Key | 용도 | 기본값 (한국어) |
| :--- | :--- | :--- |
| `app_title` | 설정 창 타이틀바 | `AN7 - 7-Zip & NanaZip Adapter` |
| `menu_extract` | 탐색기 압축 풀기 메뉴명 | `알아서 풀기` |
| `menu_new_folder` | 탐색기 새 폴더 메뉴명 | `새 폴더` |
| `folder_default_name` | 생성될 기본 폴더명 | `새 폴더` |
| `folder_date_format` | 날짜 형식 폴더명 포맷 | `새 폴더 (%04d-%02d-%02d)` |
| `input_title` | 폴더명 입력창 타이틀 | `새 폴더` |
| `input_prompt` | 폴더명 입력창 안내 문구 | `새 폴더 이름 입력:` |
| `input_hint` | 입력창 하단 안내 문구 | `Enter: 폴더 생성   \|   Esc: 취소` |
| `section_archiver` | 엔진 선택 그룹 라벨 | `[ 압축 해제 엔진 선택 (7-Zip 계열) ]` |
| `section_folder` | 폴더 설정 그룹 라벨 | `[ 새 폴더 ] 우클릭 배경 메뉴 설정` |
| `status_active` | 등록된 엔진 상태 표시 | ` [현재 등록됨]` |
| `chk_folder` | 새 폴더 등록 체크박스 | `우클릭 배경 메뉴에 '새 폴더' 등록하기` |
| `lbl_mode` | 생성 방식 라벨 | `생성 방식:` |
| `mode_input` | 팝업 모드 라디오 버튼 | `이름 변경 팝업창 띄우기 (화면 중앙 / 시스템 테마 연동)` |
| `mode_direct` | 즉시 생성 모드 라디오 버튼 | `팝업 없이 그냥 즉시 생성하기` |
| `lbl_name` | 기본 이름 라벨 | `기본 이름:` |
| `name_normal` | 일반 이름 라디오 버튼 | `기본 ("새 폴더")` |
| `name_date` | 날짜 포함 라디오 버튼 | `날짜 포함 ("새 폴더 (YYYY-MM-DD)")` |
| `btn_apply` | 적용 버튼 | `우클릭 메뉴 적용` |
| `btn_unregister` | 전체 해제 버튼 | `전체 해제` |
| `btn_close` | 닫기 버튼 | `닫기` |
| `msg_success` | 등록 완료 알림 메시지 | `우클릭 메뉴 등록이 완료되었습니다!` |
| `msg_unregistered` | 등록 해제 알림 메시지 | `모든 우클릭 메뉴 등록이 해제되었습니다.` |

### 3-3. 영문 구성 예시 (`lang.json`)

영문으로 인터페이스를 구성하고자 할 경우, 실행 파일과 동일한 경로에 `lang.json` 파일을 UTF-8 인코딩으로 저장합니다:

```json
{
  "app_title": "Smart Archiver - Archiver & Folder Settings",
  "menu_extract": "Smart Extract",
  "menu_new_folder": "New Folder",
  "folder_default_name": "New Folder",
  "folder_date_format": "New Folder (%04d-%02d-%02d)",
  "input_title": "New Folder",
  "input_prompt": "Enter folder name:",
  "input_hint": "Enter: Create   |   Esc: Cancel",
  "section_archiver": "[ Select Extraction Engine (7-Zip Family) ]",
  "section_folder": "[ New Folder ] Context Menu Settings",
  "status_active": " [Registered]",
  "chk_folder": "Register 'New Folder' to background context menu",
  "lbl_mode": "Mode:",
  "lbl_name": "Default Name:",
  "mode_input": "Show name change popup (Center / System Theme)",
  "mode_direct": "Create immediately without popup",
  "name_normal": "Default (\"New Folder\")",
  "name_date": "With Date (\"New Folder (YYYY-MM-DD)\")",
  "btn_apply": "Apply",
  "btn_unregister": "Unregister All",
  "btn_close": "Close",
  "msg_success": "Context menu registered successfully!",
  "msg_unregistered": "All context menus have been unregistered."
}
```

---

## 4. 빌드 환경

- **플랫폼**: Windows 데스크톱 (C++17 권장)
- **개발 환경**: Visual Studio 2019 / 2022
- **프로젝트 구성**: C++ Windows 애플리케이션 (Release, x64)
- **리소스 설정**:
  - 리소스 ID 130: 7-Zip 아이콘 (`7z.ico`)
  - 리소스 ID 131: NanaZip 아이콘 (`nana.ico`)

---

## 5. 사용 방법

1. **메뉴 등록**:
   - `AN7.exe`를 실행하여 설정 창을 엽니다.
   - 탐색기 메뉴에 연동할 압축 엔진 및 새 폴더 옵션을 선택합니다.
   - **[우클릭 메뉴 적용]**을 클릭하여 레지스트리에 등록합니다.

2. **압축 해제**:
   - 지원되는 압축 파일(.zip, .7z, .rar 등)을 우클릭한 후 **`알아서 풀기`**를 선택합니다.

3. **새 폴더 생성**:
   - 폴더 빈 공간이나 바탕화면을 우클릭한 후 **`새 폴더`**를 선택합니다.

4. **등록 해제**:
   - `AN7.exe` 실행 후 **[전체 해제]**를 누르면 등록된 레지스트리 항목이 제거됩니다.

---

## 6. 라이선스 및 오픈소스 고지 (License)

- 본 프로그램은 개인 및 기업 사용자 모두에게 무료로 제공됩니다.
- 본 프로그램이 연동하는 외부 압축 엔진(7-Zip, NanaZip 등)의 저작권 및 라이선스는 각 해당 원작자에게 있습니다.
