#include <sl12/descriptor.h>

#include <sl12/descriptor_heap.h>


namespace sl12
{
	//----
	void Descriptor::Destroy()
	{
		assert(pParentHeap_ == nullptr);
	}

	//----
	void Descriptor::Release()
	{
		if (!pParentHeap_)
		{
			pParentHeap_->ReleaseDescriptor(this);
			pParentHeap_ = nullptr;
		}
	}

}	// namespace sl12

//	EOF
