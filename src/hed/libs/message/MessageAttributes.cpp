#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// MessageAttributes.h

#include "MessageAttributes.h"

namespace Arc {

  AttributeIterator::AttributeIterator(){
  }

  AttributeIterator::AttributeIterator(AttrConstIter begin,
				       AttrConstIter end) :
    current_(begin), end_(end) {
  }
  
  const std::string& AttributeIterator::operator*() const {
    return current_->second;
  }

  const std::string* AttributeIterator::operator->() const {
    return &(current_->second);
  }

  const std::string& AttributeIterator::key(void) const {
    return current_->first;
  }

  const AttributeIterator& AttributeIterator::operator++() {
    ++current_;
    return *this;
  }

  AttributeIterator AttributeIterator::operator++(int) {
    AttrConstIter recent=current_++;
    return AttributeIterator(recent,end_);
  }

  bool AttributeIterator::hasMore() const {
    return current_!=end_;
  }

  MessageAttributes::MessageAttributes() {
  }

  void MessageAttributes::set(const std::string& key,
			      const std::string& value) {
    removeAll(key);
    add(key, value);
  }

  void MessageAttributes::add(const std::string& key,
			      const std::string& value) {
    attributes_.insert(make_pair(key, value));
  }

  void MessageAttributes::removeAll(const std::string& key) {
    attributes_.erase(key);
  }

  void MessageAttributes::remove(const std::string& key,
				 const std::string& value) {
    AttrIter end   = attributes_.upper_bound(key);
    for (AttrIter i=attributes_.lower_bound(key); i!=end;)
      if (i->second==value) {
	attributes_.erase(i);
        i = attributes_.lower_bound(key);
      } else {
        ++i;
      }
  }

  int MessageAttributes::count(const std::string& key) const {
    return attributes_.count(key);
  }

  const std::string& MessageAttributes::get(const std::string& key) const {
    static std::string emptyString="";
    if (count(key)==1)
      return attributes_.find(key)->second;
    else
      return emptyString; // Throw an exception?
  }

  AttributeIterator MessageAttributes::getAll(const std::string& key) const {
    return AttributeIterator(attributes_.lower_bound(key),
			     attributes_.upper_bound(key));
  }

  AttributeIterator MessageAttributes::getAll(void) const {
    return AttributeIterator(attributes_.begin(),
			     attributes_.end());
  }

}
