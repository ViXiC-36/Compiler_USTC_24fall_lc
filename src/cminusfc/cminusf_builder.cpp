#include "cminusf_builder.hpp"
#include "logging.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *INT32PTR_T;
Type *FLOAT_T;
Type *FLOATPTR_T;

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

Value* CminusfBuilder::visit(ASTProgram &node) {
    VOID_T = module->get_void_type();
    INT1_T = module->get_int1_type();
    INT32_T = module->get_int32_type();
    INT32PTR_T = module->get_int32_ptr_type();
    FLOAT_T = module->get_float_type();
    FLOATPTR_T = module->get_float_ptr_type();

    Value *ret_val = nullptr;
    for (auto &decl : node.declarations) {
        ret_val = decl->accept(*this);
    }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTNum &node) {
    // TODO: This function is empty now.
    // Add some code here.
    if (node.type == TYPE_INT) {
        context.val = CONST_INT(node.i_val);
        // LOG(DEBUG) << "INTxwx: " << node.i_val;
    } else {
        context.val = CONST_FP(node.f_val);
        // LOG(DEBUG) << "FLOATxwx: " << node.f_val;
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTVarDeclaration &node) {
    // TODO: This function is empty now.
    // Add some code here.
    Value *varAlloca;
    Type *var_type;
    if (node.num == nullptr) {
        if (node.type == TYPE_INT) {
            var_type = INT32_T;
        } else {
            var_type = FLOAT_T;
        }
    } else {
        if (node.type == TYPE_INT) {
            var_type = ArrayType::get(INT32_T, node.num->i_val); // !!!
        } else {
            var_type = ArrayType::get(FLOAT_T, node.num->i_val); // !!!
        }
    }
    // LOG(DEBUG) << "var type xwx: " << var_type->print();
    // Global or not
    if (scope.in_global()) {
        auto initializer = ConstantZero::get(var_type, module.get());
        varAlloca = GlobalVariable::create(node.id, module.get(), var_type,
                                           false, initializer);
    } else {
        varAlloca = builder->create_alloca(var_type);
    }
    scope.push(node.id, varAlloca);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTFunDeclaration &node) {
    FunctionType *fun_type;
    Type *ret_type;
    std::vector<Type *> param_types;
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    for (auto &param : node.params) {
        // TODO: Please accomplish param_types.
        // params -> param-list | void
        // param-list -> param-list, param | param
        // param -> type-specifier ID | type-specifier ID [ ]
        if (param->type == TYPE_INT) {
            if (param->isarray) {
                param_types.push_back(INT32PTR_T);
            } else {
                param_types.push_back(INT32_T);
            }
        } else {
            if (param->isarray) {
                param_types.push_back(FLOATPTR_T);
            } else {
                param_types.push_back(FLOAT_T);
            }
        }
    }

    fun_type = FunctionType::get(ret_type, param_types);
    auto func = Function::create(fun_type, node.id, module.get());
    scope.push(node.id, func);
    context.func = func;
    auto funBB = BasicBlock::create(module.get(), "entry", func);
    builder->set_insert_point(funBB);
    scope.enter();
    std::vector<Value *> args;
    for (auto &arg : func->get_args()) {
        args.push_back(&arg);
    }
    for (unsigned int i = 0; i < node.params.size(); ++i) {
        // TODO: You need to deal with params and store them in the scope.
        Type *param_type;
        if (node.params[i]->type == TYPE_INT) {
            if (node.params[i]->isarray) {
                param_type = INT32PTR_T;
            } else {
                param_type = INT32_T;
            }
        } else {
            if (node.params[i]->isarray) {
                param_type = FLOATPTR_T;
            } else {
                param_type = FLOAT_T;
            }
        }
        auto argAlloca = builder->create_alloca(param_type);
        builder->create_store(args[i], argAlloca);
        scope.push(node.params[i]->id, argAlloca);
    }
    node.compound_stmt->accept(*this);
    if (not builder->get_insert_block()->is_terminated()) 
    {
        if (context.func->get_return_type()->is_void_type())
            builder->create_void_ret();
        else if (context.func->get_return_type()->is_float_type())
            builder->create_ret(CONST_FP(0.));
        else
            builder->create_ret(CONST_INT(0));
    }
    scope.exit();
    return nullptr;
}

Value* CminusfBuilder::visit(ASTParam &node) {
    // TODO: This function is empty now.
    // Add some code here.
    // meow~
    return nullptr;
}

Value* CminusfBuilder::visit(ASTCompoundStmt &node) {
    // TODO: This function is not complete.
    // You may need to add some code here
    // to deal with complex statements. 
    scope.enter();
    for (auto &decl : node.local_declarations) {
        decl->accept(*this);
    }

    for (auto &stmt : node.statement_list) {
        stmt->accept(*this);
        if (builder->get_insert_block()->is_terminated())
            break;
    }
    scope.exit();
    return nullptr;
}

Value* CminusfBuilder::visit(ASTExpressionStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.
     // expression-stmt -> expression ; | ;
    if (node.expression != nullptr)
        node.expression->accept(*this);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTSelectionStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.
    // selection-stmt -> if ( expression ) statement
    //                  | if ( expression ) statement else statement
    node.expression->accept(*this);
    auto ret_val = context.val;
    // if
    auto trueBB = BasicBlock::create(module.get(), "", context.func);
    // out of the if else
    auto exterBB = BasicBlock::create(module.get(), "", context.func);
    // else
    auto falseBB = BasicBlock::create(module.get(), "", context.func);
    Value *cond_val; // condition value
    if (ret_val->get_type()->is_integer_type()) {
        cond_val = builder->create_icmp_ne(ret_val, CONST_INT(0));
    } else {
        cond_val = builder->create_fcmp_ne(ret_val, CONST_FP(0.));
    }
    if (node.else_statement == nullptr){
        builder->create_cond_br(cond_val, trueBB, exterBB);
    } else {
        builder->create_cond_br(cond_val, trueBB, falseBB);
    }

    builder->set_insert_point(trueBB);
    node.if_statement->accept(*this);
    if (not builder->get_insert_block()->is_terminated()){
        builder->create_br(exterBB);
    }

    if (node.else_statement == nullptr){
        falseBB->erase_from_parent();
    } else {
        builder->set_insert_point(falseBB);
        node.else_statement->accept(*this);
        if (not builder->get_insert_block()->is_terminated())
            builder->create_br(exterBB);
    }
    builder->set_insert_point(exterBB);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTIterationStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.
    // iteration-stmt -> while ( expression ) statement
    auto condBB = BasicBlock::create(module.get(), "", context.func);
    if (not builder->get_insert_block()->is_terminated())
        builder->create_br(condBB);
    builder->set_insert_point(condBB);
    node.expression->accept(*this);

    auto ret_val = context.val;
    auto trueBB = BasicBlock::create(module.get(), "", context.func);
    auto exterBB = BasicBlock::create(module.get(), "", context.func);
    Value *cond_val; // condition value
    if (ret_val->get_type()->is_integer_type()) {
        cond_val = builder->create_icmp_ne(ret_val, CONST_INT(0));
    } else {
        cond_val = builder->create_fcmp_ne(ret_val, CONST_FP(0.));
    }

    builder->create_cond_br(cond_val, trueBB, exterBB);
    builder->set_insert_point(trueBB);
    node.statement->accept(*this);
    if (not builder->get_insert_block()->is_terminated())
        builder->create_br(condBB);
    builder->set_insert_point(exterBB);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTReturnStmt &node) {
    if (node.expression == nullptr) {
        builder->create_void_ret();
        return nullptr;
    } else {
        // TODO: The given code is incomplete.
        // You need to solve other return cases (e.g. return an integer).
        // return-stmt -> return ; | return expression ;
        auto fun_ret_type = context.func->get_return_type();
        node.expression->accept(*this);
        // type check and transform
        if (fun_ret_type != context.val->get_type()) {
            if (fun_ret_type->is_integer_type()) {
                context.val = builder->create_fptosi(context.val, INT32_T);
            } else {
                context.val = builder->create_sitofp(context.val, FLOAT_T);
            }
        }
        builder->create_ret(context.val);
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTVar &node) {
    // TODO: This function is empty now.
    // Add some code here.
    context.val = scope.find(node.id);
    if (node.expression == nullptr) {
        if (!context.is_lhs) {
            if (context.val->get_type()->get_pointer_element_type()
                    ->is_array_type()) {
                context.val = builder->create_gep(context.val,
                                                  {CONST_INT(0), CONST_INT(0)});
            } else {
                context.val = builder->create_load(context.val);
            }
        }
    } else {
        Value *lhs = context.val;
        bool post_lhs = context.is_lhs;
        context.is_lhs = false;
        node.expression->accept(*this);
        context.is_lhs = post_lhs;
        Value *rhs = context.val;
        if (rhs->get_type()->is_float_type()) {
            rhs = builder->create_fptosi(rhs, INT32_T);
        }
        auto trueBB = BasicBlock::create(module.get(), "", context.func);
        auto falseBB = BasicBlock::create(module.get(), "", context.func);
        auto exterBB = BasicBlock::create(module.get(), "", context.func);
        Value *icmp = builder->create_icmp_ge(rhs, CONST_INT(0));
        builder->create_cond_br(icmp, trueBB, falseBB);
        builder->set_insert_point(trueBB);
        if (lhs->get_type()->get_pointer_element_type()->is_integer_type() ||
            lhs->get_type()->get_pointer_element_type()->is_float_type()) {
            context.val = builder->create_gep(lhs, {rhs});
        } else if (lhs->get_type()
                       ->get_pointer_element_type()
                       ->is_pointer_type()) {
            lhs = builder->create_load(lhs);
            context.val = builder->create_gep(lhs, {rhs});
        } else {
            context.val = builder->create_gep(lhs, {CONST_INT(0), rhs});
        }
        if (!context.is_lhs) {
            context.val = builder->create_load(context.val);
        }
        builder->create_br(exterBB);
        builder->set_insert_point(falseBB);
        builder->create_call(scope.find("neg_idx_except"), {});
        builder->create_br(exterBB);
        builder->set_insert_point(exterBB);
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAssignExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    // assignment-expression -> var = expression
    context.is_lhs = true;
    node.var->accept(*this);
    context.is_lhs = false;
    Value *lhs = context.val;
    node.expression->accept(*this);
    Value *rhs = context.val;
    // type transform
    if (lhs->get_type()->get_pointer_element_type() != rhs->get_type()) {
        if (rhs->get_type()->is_integer_type()) {
            context.val = builder->create_sitofp(rhs, FLOAT_T);
        } else {
            context.val = builder->create_fptosi(rhs, INT32_T);
        }
    }
    builder->create_store(context.val, lhs);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTSimpleExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    // simple-expression -> additive-expression relop additive-expression |
    // additive-expression
    if (node.additive_expression_r == nullptr) {
        node.additive_expression_l->accept(*this);
    } else {
        CminusType type = TYPE_INT;
        node.additive_expression_l->accept(*this);
        Value *lhs = context.val;
        node.additive_expression_r->accept(*this);
        Value *rhs = context.val;
        // to float if one of the operands is float
        if (lhs->get_type()->is_float_type() ||
            rhs->get_type()->is_float_type()) {
            type = TYPE_FLOAT;
            if (lhs->get_type()->is_integer_type()) {
                lhs = builder->create_sitofp(lhs, FLOAT_T);
            }
            if (rhs->get_type()->is_integer_type()) {
                rhs = builder->create_sitofp(rhs, FLOAT_T);
            }
        }
        switch (node.op) {
        case OP_LE:
            if (type == TYPE_INT) {
                context.val = builder->create_icmp_le(lhs, rhs);
            } else {
                context.val = builder->create_fcmp_le(lhs, rhs);
            }
            break;
        case OP_LT:
            if (type == TYPE_INT) {
                context.val = builder->create_icmp_lt(lhs, rhs);
            } else {
                context.val = builder->create_fcmp_lt(lhs, rhs);
            }
            break;
        case OP_GE:
            if (type == TYPE_INT) {
                context.val = builder->create_icmp_ge(lhs, rhs);
            } else {
                context.val = builder->create_fcmp_ge(lhs, rhs);
            }
            break;
        case OP_GT:
            if (type == TYPE_INT) {
                context.val = builder->create_icmp_gt(lhs, rhs);
            } else {
                context.val = builder->create_fcmp_gt(lhs, rhs);
            }
            break;
        case OP_EQ:
            if (type == TYPE_INT) {
                context.val = builder->create_icmp_eq(lhs, rhs);
            } else {
                context.val = builder->create_fcmp_eq(lhs, rhs);
            }
            break;
        case OP_NEQ:
            if (type == TYPE_INT) {
                context.val = builder->create_icmp_ne(lhs, rhs);
            } else {
                context.val = builder->create_fcmp_ne(lhs, rhs);
            }
            break;
        default:
            break;
        }
        context.val = builder->create_zext(context.val, INT32_T);
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAdditiveExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    // additive-expression -> additive-expression addop term | term
    if (node.additive_expression == nullptr) {
        node.term->accept(*this);
    } else {
        CminusType type = TYPE_INT;
        node.additive_expression->accept(*this);
        Value *lhs = context.val;
        node.term->accept(*this);
        Value *rhs = context.val;
        // to float if one of the operands is float
        if (lhs->get_type()->is_float_type() ||
            rhs->get_type()->is_float_type()) {
            type = TYPE_FLOAT;
            if (lhs->get_type()->is_integer_type()) {
                lhs = builder->create_sitofp(lhs, FLOAT_T);
            }
            if (rhs->get_type()->is_integer_type()) {
                rhs = builder->create_sitofp(rhs, FLOAT_T);
            }
        }
        switch (node.op) {
        case OP_PLUS:
            if (type == TYPE_INT) {
                context.val = builder->create_iadd(lhs, rhs);
            } else {
                context.val = builder->create_fadd(lhs, rhs);
            }
            break;
        case OP_MINUS:
            if (type == TYPE_INT) {
                context.val = builder->create_isub(lhs, rhs);
            } else {
                context.val = builder->create_fsub(lhs, rhs);
            }
            break;
        default:
            break;
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTTerm &node) {
    // TODO: This function is empty now.
    // Add some code here.
    // term -> term mulop factor | factor
    if (node.term == nullptr) {
        node.factor->accept(*this);
    } else {
        CminusType type = TYPE_INT;
        node.term->accept(*this);
        Value *lhs = context.val;
        node.factor->accept(*this);
        Value *rhs = context.val;
        // to float if one of the operands is float
        if (lhs->get_type()->is_float_type() ||
            rhs->get_type()->is_float_type()) {
            type = TYPE_FLOAT;
            if (lhs->get_type()->is_integer_type()) {
                lhs = builder->create_sitofp(lhs, FLOAT_T);
            }
            if (rhs->get_type()->is_integer_type()) {
                rhs = builder->create_sitofp(rhs, FLOAT_T);
            }
        }
        switch (node.op) {
        case OP_MUL:
            if (type == TYPE_INT) {
                context.val = builder->create_imul(lhs, rhs);
            } else {
                context.val = builder->create_fmul(lhs, rhs);
            }
            break;
        case OP_DIV:
            if (type == TYPE_INT) {
                context.val = builder->create_isdiv(lhs, rhs);
            } else {
                context.val = builder->create_fdiv(lhs, rhs);
            }
            break;
        default:
            break;
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTCall &node) {
    // TODO: This function is empty now.
    // Add some code here.
    // call -> ID ( args )
    // args -> arg-list | empty
    // arg-list -> arg-list, expression | expression
    Function *fun = (Function *)(scope.find(node.id));
    LOG(DEBUG) << "Generated IR for function call to " << node.id << ": " << fun->print();
    std::vector<Value *> args;
    auto param_iter = fun->get_function_type()->param_begin();
    for (auto &arg : node.args) {
        arg->accept(*this);
        if (context.val->get_type() != *param_iter) {
            if (context.val->get_type()->is_integer_type()) {
                context.val = builder->create_sitofp(context.val, *param_iter);
            } else {
                context.val = builder->create_fptosi(context.val, *param_iter);
            }
        }
        args.push_back(context.val);
        param_iter++;
    }
    context.val = builder->create_call(fun, args);
    return nullptr;
}
