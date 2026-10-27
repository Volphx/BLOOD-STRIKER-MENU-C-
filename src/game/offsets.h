#pragma once
#include <cstdint>

namespace offsets {
namespace engine {
inline constexpr uintptr_t WorldVtableRva = 0x4ECA2E0;
inline constexpr uintptr_t BoneNameSymbol = 0x58;
inline constexpr uintptr_t SymbolLength = 0xC;
inline constexpr uintptr_t SymbolText = 0x18;
inline constexpr uintptr_t ActorLink = 0xE8;
inline constexpr uintptr_t PoseCache = 0xD8;
inline constexpr uintptr_t PoseVector = 0x10;
inline constexpr uintptr_t BoneStride = 0xA0;
inline constexpr uintptr_t BoneResource = 0x88;
inline constexpr uintptr_t EntityWorld = 0x98;
inline constexpr uintptr_t WorldLevelsHead = 0x160;
inline constexpr uintptr_t LevelRootArea = 0xF0;
inline constexpr uintptr_t AreaEntitiesHead = 0xB8;
inline constexpr uintptr_t AreaConnectionsHead = 0xF8;
inline constexpr uintptr_t EntityHeaderSize = 0xB0;
inline constexpr uintptr_t EntityComponents = 0x40;
inline constexpr uintptr_t SkeletonSlot = 0xA1;
inline constexpr uintptr_t CameraFarPlane = 0x45C;
inline constexpr uintptr_t CameraFov = 0x454;
inline constexpr uintptr_t CameraDimensions = 0x408;
inline constexpr uintptr_t ActorHeaderSize = 0xF0;
inline constexpr uintptr_t ActorOwner = 0x40;
inline constexpr uintptr_t EntityTransform = 0x58;
inline constexpr uintptr_t BonePosition = 0x54;
inline constexpr uintptr_t BoneParent = 0x94;
inline constexpr uintptr_t CameraHeaderSize = 0x458;
inline constexpr uintptr_t CameraView = 0x41C;
inline constexpr uintptr_t WorldPosition = 0x7C;
} // namespace engine
namespace python {
inline constexpr uintptr_t RuntimeRva = 0x925F000;
inline constexpr uintptr_t ExpectedVersion = 0x30E00F0;
inline constexpr uintptr_t ObjectType = 0x8;
inline constexpr uintptr_t TypeName = 0x18;
inline constexpr uintptr_t UnicodeLength = 0x10;
inline constexpr uintptr_t UnicodeState = 0x20;
inline constexpr uintptr_t UnicodeAsciiData = 0x28;
inline constexpr uintptr_t DictKeys = 0x20;
inline constexpr uintptr_t DictValues = 0x28;
inline constexpr uintptr_t KeysEntryCount = 0x18;
inline constexpr uintptr_t KeysHeaderSize = 0x20;
inline constexpr uintptr_t SplitValuesHeader = 0x8;
inline constexpr uintptr_t TypeDictOffset = 0x120;
inline constexpr uintptr_t TypeFlags = 0xA8;
inline constexpr uintptr_t ManagedDictPreheader = 0x18;
inline constexpr uintptr_t EntityState = 0x20;
inline constexpr uintptr_t StateMirror = 0x18;
inline constexpr uintptr_t TypeDictionary = 0x108;
inline constexpr uintptr_t PropertyIndexData = 0x20;
inline constexpr uintptr_t IndexSentinel = 0x18;
inline constexpr uintptr_t NodeNext = 0x8;
inline constexpr uintptr_t StateStore = 0x20;
inline constexpr uintptr_t StoreRecord = 0x10;
inline constexpr uintptr_t RecordContainer = 0x18;
inline constexpr uintptr_t ContainerValues = 0x18;
inline constexpr uintptr_t ValueStride = 0x10;
inline constexpr uintptr_t ValueKind = 0x8;
inline constexpr uintptr_t NativeStringLength = 0x10;
inline constexpr uintptr_t NativeStringCapacity = 0x18;
inline constexpr uintptr_t WrapperHolder = 0x20;
inline constexpr uintptr_t HolderNative = 0x18;
inline constexpr uintptr_t RuntimeVersion = 0x8;
inline constexpr uintptr_t RuntimeGil = 0x10;
inline constexpr uintptr_t RuntimeInterpreters = 0x328;
inline constexpr uintptr_t InterpreterModules = 0x1DE8;
inline constexpr uintptr_t ModuleDictionary = 0x10;
} // namespace python
} // namespace offsets
