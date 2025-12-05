## 협업용 문서: Lua require 시스템 사용법

왜 엔진 함수를 globals에 노출했나?

문제 상황:
[스크립트A.lua] ──(Actor에 직접 붙음)──> sol::environment (GetComponent 등 접근 가능)
       │
       └── require("MyModule")
                    │
                    v
           [MyModule.lua] ──(global 환경에서 실행)──> GetComponent가 nil!

Lua의 require는 모듈을 전역(global) 환경에서 로드
하지만 우리 엔진은 스크립트마다 sol::environment를 만들어서 엔진 함수들을 그 안에만 노출하고 있었음.
→ require로 로드된 모듈은 environment가 아닌 global을 보기 때문에 GetComponent가 nil이 됨.

**해결**:
**LuaManager.cpp에서 엔진 함수들을 global에도 노출:**

```c++
// LuaManager에서 해줬음.

// environment에만 있던 함수들을 global에도 복사
(*Lua)["GetComponent"] = SharedLib["GetComponent"];
(*Lua)["FindObjectByName"] = SharedLib["FindObjectByName"];
// ... 기타 함수들
```

---
**require 사용법(LuaManager에서 이미 작업 완료됨)**

**package.path 설정**: 
Data/Scripts/?.lua
Data/Scripts/?/init.lua

**모듈 작성 방법** (Game/w14_MyModule.lua):

```c++
local M = {}

function M.DoSomething()
    -- 엔진 함수 사용 가능
    local obj = FindObjectByName("SomeActor")
    local comp = GetComponent(obj, "UStaticMeshComponent")
end

return M
```

**모듈 사용 방법**:

```c++
local MyModule = require("Game/w14_MyModule")  -- 확장자 생략, / 경로 사용

MyModule.DoSomething()
```

---
**사용 가능한 엔진 함수 (globals)**

| 함수                                   | 설명                        |
| -------------------------------------- | --------------------------- |
| GetComponent(actor, "UComponentName")  | Actor에서 컴포넌트 가져오기 |
| AddComponent(actor, "UComponentName")  | Actor에 컴포넌트 추가       |
| FindObjectByName("ActorName")          | 이름으로 Actor 찾기         |
| FindComponentByName("CompName")        | 이름으로 컴포넌트 찾기      |
| SpawnPrefab("path", location)          | 프리팹 스폰                 |
| DeleteObject(actor)                    | Actor 삭제                  |
| GetCamera()                            | 현재 카메라 가져오기        |
| GetCameraManager()                     | 카메라 매니저 가져오기      |
| Vector(x, y, z)                        | 벡터 생성                   |
| SetSlomo(scale)                        | 슬로모션 설정               |
| HitStop(duration) / TargetHitStop(...) | 히트스탑                    |

---
**주의사항**

1. require 경로에 .lua 생략 - require("Game/w14_Module") (O), require("Game/w14_Module.lua") (X)
2. 경로 구분자는 / - Windows에서도 / 사용
3. 모듈은 캐시됨 - 같은 모듈을 여러 번 require해도 한 번만 로드
4. return 필수 - 모듈 끝에 return M 잊지 말 것