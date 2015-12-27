/* 
 * File:   concurrent_queue.h
 * Author: andrew
 *
 * Created on 26 декабря 2015 г., 2:42
 */

#pragma once

#include <cstddef>
#include <atomic>


namespace CONC
{
    template <typename TYPE>
    class conc_queue
    {
        template <typename VALTYPE>
        struct TNode
        {
            std::atomic< TNode<TYPE> * > pnext;
            char val[sizeof(VALTYPE)] ;

            const VALTYPE * getValPtr() const { return reinterpret_cast<VALTYPE*>(&val); }
            VALTYPE * getValPtr() { return reinterpret_cast<VALTYPE*>(&val); }

            TNode()
            : 
            pnext(nullptr)
            {}
        };

        // constexpr getTearOffValue() { return reinterpret_cast< TNode<TYPE> * >( 1 ); }

        std::atomic< TNode<TYPE> * > phead_;
        std::atomic< TNode<TYPE> * > ptail_;
        std::atomic_size_t size_;
        std::atomic_size_t pop_collision_count_;
        std::atomic_size_t push_collision_count_0_;
        std::atomic_size_t push_collision_count_;

    public:
        conc_queue()
        {
            phead_ = new TNode<TYPE>();
            ptail_.store( phead_ );
            size_ = 0;
            pop_collision_count_ = 0;
            push_collision_count_ = 0;
            push_collision_count_0_ = 0;
        }

        ~conc_queue()
        {
            clear();

            // destroy last item
            TNode<TYPE> * plast = phead_.load();
            phead_.store(nullptr);
            ptail_.store(nullptr);

            delete plast;
        }

        void push( const TYPE& val )
        {
            TNode<TYPE> * pnewnode = new TNode<TYPE>();

            for(;;)
            {
                TNode<TYPE> * save_tail = ptail_.load();
                if ( ptail_.compare_exchange_strong( save_tail, pnewnode ) )
                {
                    new (save_tail->getValPtr()) TYPE( std::move( val ) ); // placement construction
                    save_tail->pnext.store( pnewnode );
                    size_ ++;
                    return;
                }
                else
                {
                    // collision happened
                    push_collision_count_ ++;
                }
            }
        }

        bool empty() const 
        {
            return phead_.load() == ptail_.load();
        }

        bool pop( TYPE* pval = nullptr )
        {
            TNode<TYPE> * save_phead;

            while ( (save_phead = phead_.load()) != ptail_.load() )
            {
                if ( phead_.compare_exchange_strong( save_phead, nullptr ) )
                {
                    if ( save_phead != nullptr )
                    {
                        if ( save_phead->pnext == nullptr )
                        {
                            phead_.store(save_phead);
                            return false; // queue is considered to be empty
                        }

                        phead_.store( save_phead->pnext ) ;
                        if (pval)
                            *pval = *save_phead->getValPtr();
                        save_phead->getValPtr()->~TYPE(); // desctuctor direct call
                        delete save_phead;
                        size_ --;
                        return true;
                    }
                    else
                    {
                        // collision happened
                        pop_collision_count_ ++;
                        push_collision_count_0_ ++;
                    }
                }
                else
                {
                    // collision happened
                    pop_collision_count_ ++;
                }
            }


            return false;
        }

        void clear()
        {
            while (pop());
        }

        std::size_t getPushCollisionCount() const 
        { 
            return push_collision_count_.load(); 
        }

        std::size_t getPopCollisionCount() const 
        { 
            return pop_collision_count_.load(); 
        }
        
        std::size_t getPopCollisionCount0() const 
        {
            return push_collision_count_0_.load();
        }

        std::size_t size() const { return size_.load(); }
    };
    
}

