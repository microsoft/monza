// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

namespace monza
{
  template<typename T>
  class Queue
  {
    struct QueueEntry
    {
      T object;
      QueueEntry* next;

      QueueEntry(T&& temp_object, QueueEntry* next)
      : object(std::move(temp_object)), next(next)
      {}
    };

    QueueEntry* head = nullptr;
    QueueEntry* tail = nullptr;

  public:
    Queue(Queue&& other) : head(std::move(other.head)), tail(other.tail)
    {
      other.head = nullptr;
      other.tail = nullptr;
    }

    Queue() {}

    ~Queue()
    {
      while (!empty())
      {
        pop_front();
      }
    }

    bool empty()
    {
      return head == nullptr;
    }

    void push_back(T&& object)
    {
      auto new_entry = new QueueEntry(std::move(object), nullptr);
      if (tail != nullptr)
      {
        tail->next = new_entry;
      }
      tail = new_entry;
      if (head == nullptr)
      {
        head = new_entry;
      }
    }

    T pop_front()
    {
      auto entry = head;
      head = head->next;
      if (head == nullptr)
      {
        tail = nullptr;
      }
      auto obj = std::move(entry->object);
      delete entry;
      return obj;
    }
  };
}
