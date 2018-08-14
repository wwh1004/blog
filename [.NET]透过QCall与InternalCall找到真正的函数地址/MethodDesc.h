// The size of this structure needs to be a multiple of MethodDesc::ALIGNMENT
//
// @GENERICS:
// Method descriptors for methods belonging to instantiated types may be shared between compatible instantiations
// Hence for reflection and elsewhere where exact types are important it's necessary to pair a method desc
// with the exact owning type handle.
//
// See genmeth.cpp for details of instantiated generic method descriptors.
// 
// A MethodDesc is the representation of a method of a type.  These live in code:MethodDescChunk which in
// turn lives in code:EEClass.   They are conceptually cold (we do not expect to access them in normal
// program exectution, but we often fall short of that goal.  
// 
// A Method desc knows how to get at its metadata token code:GetMemberDef, its chunk
// code:MethodDescChunk, which in turns knows how to get at its type code:MethodTable.
// It also knows how to get at its IL code (code:IMAGE_COR_ILMETHOD)
class MethodDesc
{
    //================================================================
    // The actual data stored in a MethodDesc follows.
protected:
    enum {
        // There are flags available for use here (currently 5 flags bits are available); however, new bits are hard to come by, so any new flags bits should
        // have a fairly strong justification for existence.
        enum_flag3_TokenRemainderMask                       = 0x3FFF, // This must equal METHOD_TOKEN_REMAINDER_MASK calculated higher in this file
                                                                      // These are seperate to allow the flags space available and used to be obvious here
                                                                      // and for the logic that splits the token to be algorithmically generated based on the 
                                                                      // #define
        enum_flag3_HasForwardedValuetypeParameter           = 0x4000, // Indicates that a type-forwarded type is used as a valuetype parameter (this flag is only valid for ngenned items)
        enum_flag3_ValueTypeParametersWalked                = 0x4000, // Indicates that all typeref's in the signature of the method have been resolved to typedefs (or that process failed) (this flag is only valid for non-ngenned methods)
        enum_flag3_DoesNotHaveEquivalentValuetypeParameters = 0x8000, // Indicates that we have verified that there are no equivalent valuetype parameters for this method
    };
    UINT16      m_wFlags3AndTokenRemainder;
    
    BYTE        m_chunkIndex;

    enum {
        // enum_flag2_HasPrecode implies that enum_flag2_HasStableEntryPoint is set.
        enum_flag2_HasStableEntryPoint      = 0x01,   // The method entrypoint is stable (either precode or actual code)
        enum_flag2_HasPrecode               = 0x02,   // Precode has been allocated for this method

        enum_flag2_IsUnboxingStub           = 0x04,
        enum_flag2_HasNativeCodeSlot        = 0x08,   // Has slot for native code

        enum_flag2_IsJitIntrinsic           = 0x10,   // Jit may expand method as an intrinsic

        // unused                           = 0x20,
        // unused                           = 0x40,
        // unused                           = 0x80, 
    };
    BYTE        m_bFlags2;

    // The slot number of this MethodDesc in the vtable array.
    // Note that we may store other information in the high bits if available -- 
    // see enum_packedSlotLayout and mdcRequiresFullSlotNumber for details.
    WORD m_wSlotNumber;

    enum {
        enum_packedSlotLayout_SlotMask      = 0x03FF,
        enum_packedSlotLayout_NameHashMask  = 0xFC00
    };

    WORD m_wFlags;
};

//-----------------------------------------------------------------------
// Operations specific to NDirect methods. We use a derived class to get
// the compiler involved in enforcing proper method type usage.
// DO NOT ADD FIELDS TO THIS CLASS.
//-----------------------------------------------------------------------
class NDirectMethodDesc : public MethodDesc
{
public:
    struct temp1
    {
        // If we are hosted, stack imbalance MDA is active, or alignment thunks are needed,
        // we will intercept m_pNDirectTarget. The true target is saved here.
        LPVOID      m_pNativeNDirectTarget;
            
        // Information about the entrypoint
        RelativePointer<PTR_CUTF8>     m_pszEntrypointName;

        union
        {
            RelativePointer<PTR_CUTF8>     m_pszLibName;
            DWORD       m_dwECallID;    // ECallID for QCalls
        };

        // The writeable part of the methoddesc.
#if defined(FEATURE_NGEN_RELOCS_OPTIMIZATIONS)
        RelativePointer<PTR_NDirectWriteableData>    m_pWriteableData;
#else
        PlainPointer<PTR_NDirectWriteableData>    m_pWriteableData;
#endif

#ifdef HAS_NDIRECT_IMPORT_PRECODE
        RelativePointer<PTR_NDirectImportThunkGlue> m_pImportThunkGlue;
#else // HAS_NDIRECT_IMPORT_PRECODE
        NDirectImportThunkGlue      m_ImportThunkGlue;
#endif // HAS_NDIRECT_IMPORT_PRECODE

        ULONG       m_DefaultDllImportSearchPathsAttributeValue; // DefaultDllImportSearchPathsAttribute is saved.

        // Various attributes needed at runtime.
        WORD        m_wFlags;

#if defined(_TARGET_X86_)
        // Size of outgoing arguments (on stack). Note that in order to get the @n stdcall name decoration,
        // it may be necessary to subtract 4 as the hidden large structure pointer parameter does not count.
        // See code:kStdCallWithRetBuf
        WORD        m_cbStackArgumentSize;
#endif // defined(_TARGET_X86_)

        // This field gets set only when this MethodDesc is marked as PreImplemented
        RelativePointer<PTR_MethodDesc> m_pStubMD;

    } ndirect;

    enum Flags
    {
        // There are two groups of flag bits here each which gets initialized
        // at different times. 

        //
        // Group 1: The init group.
        //
        //   This group is set during MethodDesc construction. No race issues
        //   here since they are initialized before the MD is ever published
        //   and never change after that.

        kEarlyBound                     = 0x0001,   // IJW managed->unmanaged thunk. Standard [sysimport] stuff otherwise.

        kHasSuppressUnmanagedCodeAccess = 0x0002,

        kDefaultDllImportSearchPathsIsCached = 0x0004, // set if we cache attribute value.

        // kUnusedMask                  = 0x0008

        //
        // Group 2: The runtime group.
        //
        //   This group is set during runtime potentially by multiple threads
        //   at the same time. All flags in this category has to be set via interlocked operation.
        //
        kIsMarshalingRequiredCached     = 0x0010,   // Set if we have cached the results of marshaling required computation
        kCachedMarshalingRequired       = 0x0020,   // The result of the marshaling required computation

        kNativeAnsi                     = 0x0040,

        kLastError                      = 0x0080,   // setLastError keyword specified
        kNativeNoMangle                 = 0x0100,   // nomangle keyword specified

        kVarArgs                        = 0x0200,
        kStdCall                        = 0x0400,
        kThisCall                       = 0x0800,

        kIsQCall                        = 0x1000,

        kDefaultDllImportSearchPathsStatus = 0x2000, // either method has custom attribute or not.

        kHasCopyCtorArgs                = 0x4000,

        kStdCallWithRetBuf              = 0x8000,   // Call returns large structure, only valid if kStdCall is also set

    };
};  //class NDirectMethodDesc
