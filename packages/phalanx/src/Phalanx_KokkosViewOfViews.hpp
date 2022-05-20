#ifndef PHALANX_KOKKOS_VIEW_OF_VIEWS_HPP
#define PHALANX_KOKKOS_VIEW_OF_VIEWS_HPP

#include <utility> // for declval

namespace PHX {

  // ****************************
  // Add pointer (used to construct the static data type)
  // ****************************

  namespace v_of_v_utils {
    template<typename Data,int N> struct add_pointer;

    template<typename Data,int N> struct add_pointer
    { using type = typename add_pointer<Data*,N-1>::type; };

    template<typename Data> struct add_pointer<Data,0>
    { using type = Data; };
  }

  // ****************************
  // ViewOfViews: original version
  // ****************************

  /** Wrapper class that correctly handles ViewOfViews construction
      and object lifetime. Can be used for more than just Views as the
      inner object. This class makes sure the host view stays in scope
      for the life of the device view and makes sure that the device
      is synced to host before use.

      Main restrictions:

      1. When UVM is not used in the outer view, we need to allocate
      the VofV on host and copy to device to set up the inner views
      correctly.

      2. Step 1 means that the host view must exist as long as the
      device view is being used, otherwise the views may go out of
      scope. This object exists to pair up the host and device view to
      make sure the inner views are not deleted early.
  */
  template<int OuterViewRank,typename InnerViewType,typename MemorySpace>
  class ViewOfViews {

  public:
    // Layout of the outer view doesn't matter for performance so we
    // use a raw Kokkos::View instead of PHX::View. The inner views are
    // what is important for performance.
    using OuterDataType = typename PHX::v_of_v_utils::add_pointer<InnerViewType,OuterViewRank>::type;
    using OuterViewType = Kokkos::View<OuterDataType,MemorySpace>;

  private:
    typename OuterViewType::HostMirror view_host_;
    OuterViewType view_device_;
    bool device_view_is_synced_;

  public:
    template<typename... Extents>
    ViewOfViews(const std::string name,
                Extents... extents)
      : view_host_(name,extents...),
        view_device_(name,extents...),
        device_view_is_synced_(false)
    {
      // Inner view must be unmanaged if the outerview is not using UVM!
      static_assert(InnerViewType::memory_traits::is_unmanaged,
                    "ERROR: PHX::ViewOfViews - Inner view must be unmanaged!");
    }

    ~ViewOfViews()
    {
      // Make sure there is not another object pointing to device view
      // since the host view will delete the inner views on exit.
      if (view_device_.impl_track().use_count() != 1)
        Kokkos::abort("\n ERROR - PHX::ViewOfViews - please free all instances of device ViewOfView \n before deleting the host ViewOfView!\n\n");
    }

    template<typename... Indices>
    void addView(InnerViewType v,Indices... i)
    {
      view_host_(i...) = v;
      device_view_is_synced_ = false;
    }

    void syncHostToDevice()
    {
      Kokkos::deep_copy(view_device_,view_host_);
      device_view_is_synced_ = true;
    }

    auto getViewHost()
    {
      return view_host_;
    }

    auto getViewDevice()
    {
      TEUCHOS_ASSERT(device_view_is_synced_);
      return view_device_;
    }
  };

  // ****************************
  // ViewOfViews: new version (inner views use Unmanaged template parameter)
  // ****************************

  namespace details {

    // Trick to pull out inner view template parameters and expand the
    // view properties.
    template<typename... Props>
    auto
    getUnmanaged(const Kokkos::View<Props...>& v) {
      Kokkos::View<Props..., Kokkos::MemoryTraits<Kokkos::Unmanaged>> tmp = v;
      return tmp;
    }

  }

  /** Wrapper class that correctly handles ViewOfViews construction
      and object lifetime. This class makes sure the host view stays
      in scope for the life of the device view and makes sure that the
      device is synced to host before use.

      Main restrictions:

      1. When UVM is not used in the outer view, we need to allocate
      the VofV on host and deep_copy to device to set up the inner views
      correctly.

      2. Step 1 means that the host view must exist as long as the
      device view is being used, otherwise the inner views may go out
      of scope and delete memory. This object exists to pair up the
      host and device view to make sure the inner views are not
      deleted early.

      3. The InnerViewType template parameter must be managed. We
      will add the unmanaged tag internally.

      4. This object assumes that all inner views are on device. When
      the accessors reference Host/Device it is with respect to the
      outer view. If a user wants to initialize the inner view data,
      they must do that manually external to this object and deep_copy
      to the device views.

      @param OuterViewRank The rank of the outerview.
      @param InnerViewType The type of inner view. Currently MUST be a Managed view!
      @param OuterViewProps View properties for the outer view (i.e. space, layout, memory traits, ...).
  */
  template<int OuterViewRank,typename InnerViewType,typename... OuterViewProps>
  class ViewOfViews2 {

  public:
    using InnerViewTypeManaged = InnerViewType;
    using InnerViewTypeUnmanaged = decltype(PHX::details::getUnmanaged(std::declval<InnerViewType>()));
    using OuterViewDataTypeManagedInner = typename PHX::v_of_v_utils::add_pointer<InnerViewTypeManaged,OuterViewRank>::type;
    using OuterViewDataTypeUnmanagedInner = typename PHX::v_of_v_utils::add_pointer<InnerViewTypeUnmanaged,OuterViewRank>::type;
    using OuterViewManaged = Kokkos::View<OuterViewDataTypeManagedInner,OuterViewProps...>;
    using OuterViewUnmanaged = Kokkos::View<OuterViewDataTypeUnmanagedInner,OuterViewProps...>;
    using OuterViewManagedHostMirror = typename OuterViewManaged::HostMirror;
    using OuterViewUnmanagedHostMirror = typename OuterViewUnmanaged::HostMirror;

  private:

    // Host view with managed inner views so that the inner views
    // can't be deleted before the outer device view is deleted.
    OuterViewManagedHostMirror view_host_managed_;

    // Host view with unmanaged inner views. Needed for deep_copy to
    // device.
    OuterViewUnmanagedHostMirror view_host_unmanaged_;

    // Device view with unmanaged inner views.
    OuterViewUnmanaged view_device_;

    // True if device view is updated with host view data.
    bool device_view_is_synced_;

    // True if the outer view has been allocated via ctor or call to initialize().
    bool is_initialized_;

  public:

    template<typename... Extents>
    ViewOfViews2(const std::string name, Extents... extents)
    { this->initialize(name,extents...); }

    ViewOfViews2() :
      device_view_is_synced_(false),
      is_initialized_(false)
    {}

    ~ViewOfViews2()
    {
      // Make sure there is not another object pointing to device view
      // since the host view will delete the inner views on exit.
      if (view_device_.impl_track().use_count() != 1)
        Kokkos::abort("\n ERROR - PHX::ViewOfViews - please free all instances of device ViewOfView \n before deleting the host ViewOfView!\n\n");
    }

    /// Allocate the out view objects. Extents are for the outer view.
    template<typename... Extents>
    void initialize(const std::string name,Extents... extents)
    {
      view_host_managed_ = OuterViewManagedHostMirror(name,extents...);
      view_host_unmanaged_ = OuterViewUnmanagedHostMirror(name,extents...);
      view_device_ = OuterViewUnmanaged(name,extents...);
      device_view_is_synced_ = false;
      is_initialized_ = true;
    }

    /// Set an inner device view on the outer view. Indices are the outer view indices. 
    template<typename... Indices>
    void setView(InnerViewType v,Indices... i)
    {
      TEUCHOS_ASSERT(is_initialized_);
      view_host_managed_(i...) = v;
      view_host_unmanaged_(i...) = v;
      device_view_is_synced_ = false;
    }

    /// Note this only syncs the outer view. The inner views are
    /// assumed to be on device for both host and device outer views.
    void syncHostToDevice()
    {
      TEUCHOS_ASSERT(is_initialized_);
      Kokkos::deep_copy(view_device_,view_host_unmanaged_);
      device_view_is_synced_ = true;
    }

    /// Returns a host mirror view for the outer view, where the inner
    /// views are still on device.
    auto getViewHost()
    {
      TEUCHOS_ASSERT(is_initialized_);
      return view_host_managed_;
    }

    /// Returns device view of views
    auto getViewDevice()
    {
      KOKKOS_ASSERT(device_view_is_synced_);
      return view_device_;
    }
  };

  // ****************************
  // ViewOfViews: third version (inner views are runtime unmanaged - no Unmanaged template parameter)
  // ****************************

  /** Wrapper class that correctly handles ViewOfViews construction
      and object lifetime. This class makes sure the host view stays
      in scope for the life of the device view and makes sure that the
      device is synced to host before use.

      Main restrictions:

      1. When UVM is not used in the outer view, we need to allocate
      the outer VofV on host and copy to device to initialize the
      inner views correctly (tracking object).

      2. Step 1 means that the host view must exist as long as the
      device view is being used, otherwise the views may go out of
      scope. This object exists to pair up the host and device view to
      make sure the inner views are not deleted early.

      3. Normally we use an unmanaged view (constructed with the
      Unmanaged template parameter) for the inner views to prevent
      double deletion. However, there are use cases where it's painful
      to pass around views built with the unmanaged template parameter
      (libraries with finctions that block the unmanaged argument). We
      can generate an unmanged view without the template parameter by
      constructing the view with a raw pointer. This thrid
      implementation does that here.
  */
  template<int OuterViewRank,typename InnerViewType,typename... OuterViewProps>
  class ViewOfViews3 {

  public:
    // Layout of the outer view doesn't matter for performance so we
    // use a raw Kokkos::View instead of PHX::View. The inner views are
    // what is important for performance.
    using OuterDataType = typename PHX::v_of_v_utils::add_pointer<InnerViewType,OuterViewRank>::type;
    using OuterViewType = Kokkos::View<OuterDataType,OuterViewProps...>;

  private:
    // Inner views are mananged - used to prevent early deletion
    typename OuterViewType::HostMirror view_host_;
    // Inner views are unmanaged by runtime construction with pointer
    // (avoids template parameter). Used to correctly initialize outer
    // device view on device.
    typename OuterViewType::HostMirror view_host_unmanaged_;
    // Device view
    OuterViewType view_device_;
    // True if the host view has not been synced to device
    bool device_view_is_synced_;
    // True if the outer view has been initialized
    bool is_initialized_;

  public:
    template<typename... Extents>
    ViewOfViews3(const std::string name,Extents... extents)
      : view_host_(name,extents...),
        view_host_unmanaged_(name,extents...),
        view_device_(name,extents...),
        device_view_is_synced_(false),
        is_initialized_(true)
    {}

    ViewOfViews3()
      : device_view_is_synced_(false),
        is_initialized_(false)
    {}

    ~ViewOfViews3()
    {
      // Make sure there is not another object pointing to device view
      // since the host view will delete the inner views on exit.
      if (view_device_.impl_track().use_count() != 1)
        Kokkos::abort("\n ERROR - PHX::ViewOfViews - please free all instances of device ViewOfView \n before deleting the host ViewOfView!\n\n");
    }

    /// Allocate the out view objects. Extents are for the outer view.
    template<typename... Extents>
    void initialize(const std::string name,Extents... extents)
    {
      view_host_ = typename OuterViewType::HostMirror(name,extents...);
      view_host_unmanaged_ = typename OuterViewType::HostMirror(name,extents...);
      view_device_ = OuterViewType(name,extents...);
      device_view_is_synced_ = false;
      is_initialized_ = true;
    }

    // Returns true if the outer view has been initialized.
    bool is_initialized() {return is_initialized_;}

    template<typename... Indices>
    void addView(InnerViewType v,Indices... i)
    {
      TEUCHOS_ASSERT(is_initialized_);
      // Store the managed version so it doesn't get deleted.
      view_host_(i...) = v;
      // Store a runtime unmanaged view to prevent double deletion on device
      view_host_unmanaged_(i...) = InnerViewType(v.data(),v.layout());
      device_view_is_synced_ = false;
    }

    /// Note this only syncs the outer view. The inner views are
    /// assumed to be on device for both host and device outer views.
    void syncHostToDevice()
    {
      TEUCHOS_ASSERT(is_initialized_);
      Kokkos::deep_copy(view_device_,view_host_unmanaged_);
      device_view_is_synced_ = true;
    }

    /// Returns a host mirror view for the outer view, where the inner
    /// views are still on device.
    auto getViewHost()
    {
      TEUCHOS_ASSERT(is_initialized_);
      return view_host_;
    }

    /// Returns device view of views
    auto getViewDevice()
    {
      KOKKOS_ASSERT(device_view_is_synced_);
      return view_device_;
    }
  };

}

#endif
