
#include "zypp/repo/RepositoryImpl.h"

namespace zypp { namespace repo {

IMPL_PTR_TYPE(RepositoryImpl)

RepositoryImpl::RepositoryImpl( const RepoInfo &info )
  : _info(info)
{

}

const RepoInfo RepositoryImpl::info() const
{
  return _info;
}

RepositoryImpl::~RepositoryImpl()
{

}

RepositoryImpl::RepositoryImpl( const null & )
  : base::ProvideNumericId<RepositoryImpl,Repository::NumericId>( NULL )
{}


const ResStore & RepositoryImpl::resolvables() const
{
  return _store;
}

const std::list<packagedelta::PatchRpm> &
RepositoryImpl::patchRpms() const
{
  return _patchRpms;
}
    
const std::list<packagedelta::DeltaRpm> &
RepositoryImpl::deltaRpms() const
{
    return _deltaRpms;
}

} } // ns

