#include "FmsSpinLock.h"
#include "fms_spin_lock.h"

//��Ϊ���������ǲ��Ǹ���?


fms_void fms_spin_lock(fms_atomic *const lock_value) {
	FmsSpinLock spinLock(lock_value, FMS_FALSE);
	spinLock.lock();
}

const fms_bool fms_spin_trylock(fms_atomic *const lock_value) {
	FmsSpinLock spinLock(lock_value, FMS_FALSE);
	spinLock.trylock();	
}

fms_void fms_spin_unlock(fms_atomic *const lock_value) {
	FmsSpinLock spinLock(lock_value, FMS_FALSE);	
	spinLock.unlock();
}