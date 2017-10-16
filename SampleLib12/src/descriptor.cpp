#include <sl12/descriptor.h>

#include <sl12/descriptor_heap.h>


namespace sl12
{
	//----
	void Descriptor::Destroy()
	{
	}

	//----
	void Descriptor::Release()
	{
		if (pParentHeap_)
		{
			pParentHeap_->ReleaseDescriptor(this);
		}
	}

}	// namespace sl12

//	EOF
