#pragma once

// 인터페이스를 상속 시, 키네마틱 엑터를 가진 컴포넌트에 대해 PrePhysicsUpdate함수가 호출됨.
// ex) 시뮬레이션 돌아가기 전에 스켈레탈 메시가 애니메이션 업데이트하고 엑터 업데이트함.
// 물리 적용되도 블렌딩해서 술 취한 사람이 흐느적 거리는 연출이 가능한데 일단 배제함.
// 컴포넌트에서 함수 구현하면 PhysicsScene이 컴포넌트에 헤더 의존성 생겨서 인터페이스 만듦
class IPrePhysics
{

public:
	virtual void PrePhysicsUpdate(float DeltaTime) = 0;
};