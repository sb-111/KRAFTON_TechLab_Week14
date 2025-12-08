# PhysX Crash 디버깅 리포트

**날짜**: 2024-12-08
**증상**: 게임 오버 → 재시작 시 크래시 발생

---

## 1. 크래시 정보

### 호출 스택
```
physx::PxBase::typeMatch<physx::PxRigidBody>() 줄 182
physx::PxBase::is<physx::PxRigidBody>() 줄 101
FBodyInstance::UpdateTransform::__l2::<lambda_1>::operator() 줄 82
FPhysicsScene::ProcessCommandQueue() 줄 65
UWorld::Tick(float DeltaSeconds) 줄 300
```

### 에러 메시지
```
예외가 throw됨: 읽기 액세스 위반입니다.
physx::PxBase::isKindOf[virtual]이(가) 0xFFFFFFFFFFFFFFFF였습니다
```

`0xFFFFFFFFFFFFFFFF`는 삭제된 메모리를 나타냄.

---

## 2. 근본 원인 분석

### 문제 흐름

1. **게임 플레이 중**: `FBodyInstance::UpdateTransform()` 호출
   ```cpp
   void FBodyInstance::UpdateTransform(const FTransform& InTransform)
   {
       PxRigidActor* Actor = RigidActor;
       GWorld->GetPhysicsScene()->EnqueueCommand([Actor, InTransform] {
           if (Actor)  // ← 포인터 null 체크만 함
           {
               PxRigidBody* Body = Actor->is<PxRigidBody>();  // ← 크래시 발생 지점
               ...
           }
       });
   }
   ```
   - 람다가 `PxRigidActor* Actor` 포인터를 캡처하여 `CommandQueue`에 추가됨

2. **게임 오버 → 재시작**: `CleanupGame()` → `DeleteObject(Player)` 호출
   - `FBodyInstance::~FBodyInstance()` → `TermBody()` 실행
   - `WriteInTheDeathNote(RigidActor)` 호출
   ```cpp
   void FPhysicsScene::WriteInTheDeathNote(physx::PxActor* ActorToDie)
   {
       ...
       ActorToDie->userData = nullptr;  // ← userData를 null로 설정
       ActorToDie->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, true);
       ActorDeathNote.Add(ActorToDie);
   }
   ```

3. **다음 프레임 `World::Tick()`**:
   - `ProcessCommandQueue()` 실행
   - 이전에 캡처된 람다가 실행됨
   - `Actor` 포인터는 여전히 non-null이지만, 실제 객체는 삭제 대기 상태
   - `Actor->is<PxRigidBody>()` 호출 시 **크래시**

### 핵심 문제
- `if (Actor)` 체크는 포인터가 null인지만 확인
- 이미 `WriteInTheDeathNote`에서 삭제 예약된 액터인지는 확인하지 않음
- `WriteInTheDeathNote`에서 `userData = nullptr`로 설정하므로 이를 활용해야 함

---

## 3. 수정 내용

### 3.1 BodyInstance.cpp (핵심 수정)

모든 `EnqueueCommand` 람다에서 `userData` null 체크 추가:

```cpp
// 수정 전
if (Actor)

// 수정 후
if (Actor && Actor->userData)
```

**수정된 함수 목록:**
| 함수 | 줄 번호 |
|------|---------|
| `AddForce` | 46 |
| `AddTorque` | 64 |
| `UpdateTransform` | 82 |
| `PutToSleep` | 341 |
| `WakeUp` | 364 |

### 3.2 w14_GameMain.lua (보조 수정)

`CleanupGame()`에서 Player 삭제 전 물리 상태 비활성화:

```lua
-- 수정 전
if Player and DeleteObject then
    DeleteObject(Player)
    Player = nil
end

-- 수정 후
if Player then
    Player:SetPhysicsState(false)
    if DeleteObject then
        DeleteObject(Player)
    end
    Player = nil
end
```

### 3.3 w14_ObjectPool.lua (보조 수정)

`Pool:destroy()`에서 오브젝트 삭제 전 물리 상태 비활성화:

```lua
-- 수정 전
if obj and DeleteObject then
    DeleteObject(obj)
end

-- 수정 후
if obj then
    if obj.SetPhysicsState then
        obj:SetPhysicsState(false)
    end
    if DeleteObject then
        DeleteObject(obj)
    end
end
```

---

## 4. 수정 파일 요약

| 파일 | 수정 유형 | 설명 |
|------|-----------|------|
| `Mundi/Source/Runtime/Engine/Physics/BodyInstance.cpp` | C++ | 람다에서 `userData` null 체크 추가 |
| `Mundi/Data/Scripts/w14_GameMain.lua` | Lua | Player 삭제 전 물리 비활성화 |
| `Mundi/Data/Scripts/w14_ObjectPool.lua` | Lua | Pool 오브젝트 삭제 전 물리 비활성화 |

---

## 5. 테스트 방법

1. C++ 프로젝트 빌드
2. 게임 실행
3. 플레이 → 몬스터에게 피격되어 게임 오버
4. 스페이스바로 재시작
5. 크래시 없이 정상 재시작 확인

---

## 6. 추가 고려사항

- `userData` null 체크는 방어적 프로그래밍 관점에서 다른 `EnqueueCommand` 사용처에도 적용 권장
- 향후 PhysX 관련 코드 작성 시, 캡처된 포인터의 유효성을 반드시 확인할 것
