// This file is licensed under the Elastic License 2.0. Copyright 2021-present, StarRocks Limited.

#include "exprs/vectorized/cast_expr.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "butil/time.h"
#include "column/fixed_length_column.h"
#include "column/type_traits.h"
#include "exprs/vectorized/mock_vectorized_expr.h"
#include "gen_cpp/Exprs_types.h"
#include "gen_cpp/Types_types.h"
#include "runtime/primitive_type.h"
#include "runtime/vectorized/time_types.h"
#include "util/json.h"

namespace starrocks {
namespace vectorized {

class VectorizedCastExprTest : public ::testing::Test {
public:
    void SetUp() {
        expr_node.opcode = TExprOpcode::ADD;
        expr_node.child_type = TPrimitiveType::INT;
        expr_node.node_type = TExprNodeType::BINARY_PRED;
        expr_node.num_children = 2;
        expr_node.__isset.opcode = true;
        expr_node.__isset.child_type = true;
        expr_node.type = gen_type_desc(TPrimitiveType::BOOLEAN);
    }

public:
    TExprNode expr_node;
};

TEST_F(VectorizedCastExprTest, IntCastToDate) {
    expr_node.child_type = TPrimitiveType::INT;
    expr_node.type = gen_type_desc(TPrimitiveType::DATE);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_INT> col1(expr_node, 10, 20111101);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_date());

        auto v = ColumnHelper::cast_to_raw<TYPE_DATE>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(DateValue::create(2011, 11, 01), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, BigIntCastToTimestamp) {
    expr_node.child_type = TPrimitiveType::BIGINT;
    expr_node.type = gen_type_desc(TPrimitiveType::DATETIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_BIGINT> col1(expr_node, 10, 20220203112345);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_timestamp());

        auto v = std::static_pointer_cast<TimestampColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(TimestampValue::create(2022, 02, 03, 11, 23, 45), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, BigIntCastToTimestampError) {
    expr_node.child_type = TPrimitiveType::BIGINT;
    expr_node.type = gen_type_desc(TPrimitiveType::DATETIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_BIGINT> col1(expr_node, 10, 20220003112345);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);
        ASSERT_TRUE(ptr->is_nullable());

        ASSERT_FALSE(ptr->is_timestamp());

        ASSERT_TRUE(ColumnHelper::as_raw_column<NullableColumn>(ptr)->data_column()->is_timestamp());

        for (int j = 0; j < ptr->size(); ++j) {
            ASSERT_TRUE(ptr->is_null(j));
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<TimestampColumn>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, dateCastToBoolean) {
    expr_node.child_type = TPrimitiveType::DATE;
    expr_node.type = gen_type_desc(TPrimitiveType::BOOLEAN);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_DATE> col1(expr_node, 10, DateValue::create(123123, 1, 1));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = std::static_pointer_cast<BooleanColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(true, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, timestampCastToBoolean) {
    expr_node.child_type = TPrimitiveType::DATETIME;
    expr_node.type = gen_type_desc(TPrimitiveType::BOOLEAN);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_DATETIME> col1(expr_node, 10, TimestampValue::create(12, 1, 1, 25, 1, 1));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = std::static_pointer_cast<BooleanColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(true, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringLiteralTrueCastToBoolean) {
    expr_node.child_type = TPrimitiveType::CHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::BOOLEAN);
    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string s = "true";
    MockVectorizedExpr<TYPE_CHAR> col1(expr_node, 10, Slice(s));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = std::static_pointer_cast<BooleanColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(true, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringLiteralFalseCastToBoolean) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::BOOLEAN);
    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string s = "false";
    MockVectorizedExpr<TYPE_CHAR> col1(expr_node, 10, Slice(s));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = std::static_pointer_cast<BooleanColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(false, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringLiteralIntCastToBoolean) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::BOOLEAN);
    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string s = "1";
    MockVectorizedExpr<TYPE_CHAR> col1(expr_node, 10, Slice(s));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = std::static_pointer_cast<BooleanColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(true, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, intCastSelfExpr) {
    expr_node.child_type = TPrimitiveType::INT;
    expr_node.type = gen_type_desc(TPrimitiveType::INT);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_INT> col1(expr_node, 10, 10);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = std::static_pointer_cast<Int32Column>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(10, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, intToFloatCastExpr) {
    expr_node.child_type = TPrimitiveType::INT;
    expr_node.type = gen_type_desc(TPrimitiveType::FLOAT);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_INT> col1(expr_node, 10, 10);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = std::static_pointer_cast<FloatColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(10, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, intToInt8CastExpr) {
    expr_node.child_type = TPrimitiveType::INT;
    expr_node.type = gen_type_desc(TPrimitiveType::TINYINT);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_INT> col1(expr_node, 10, 10);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = std::static_pointer_cast<Int8Column>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(10, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, intToBigIntCastExpr) {
    expr_node.child_type = TPrimitiveType::INT;
    expr_node.type = gen_type_desc(TPrimitiveType::BIGINT);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_INT> col1(expr_node, 10, 10);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = std::static_pointer_cast<Int64Column>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(10, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int8Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, NullableBooleanCastExpr) {
    expr_node.child_type = TPrimitiveType::INT;
    expr_node.type = gen_type_desc(TPrimitiveType::BOOLEAN);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockNullVectorizedExpr<TYPE_INT> col1(expr_node, 10, 10);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_FALSE(ptr->is_numeric());
        ASSERT_TRUE(ptr->is_nullable());

        // right cast
        auto v = std::static_pointer_cast<BooleanColumn>(std::static_pointer_cast<NullableColumn>(ptr)->data_column());
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(1, (v->get_data()[j]));
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, dateCastToDecimalV2) {
    expr_node.child_type = TPrimitiveType::DATE;
    expr_node.type = gen_type_desc(TPrimitiveType::DECIMALV2);
    expr_node.type.types[0].scalar_type.__set_precision(10);
    expr_node.type.types[0].scalar_type.__set_scale(2);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_DATE> col1(expr_node, 10, DateValue::create(2000, 12, 31));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_decimal());

        // right cast
        auto v = std::static_pointer_cast<DecimalColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(DecimalV2Value(20001231, 0), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, decimalV2CastToTimestamp) {
    expr_node.child_type = TPrimitiveType::DECIMALV2;
    expr_node.type = gen_type_desc(TPrimitiveType::DATETIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_DECIMALV2> col1(expr_node, 10, DecimalV2Value("20010129123000"));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_timestamp());

        // right cast
        auto v = std::static_pointer_cast<TimestampColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(TimestampValue::create(2001, 01, 29, 12, 30, 00), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, dateCastToTimestamp) {
    expr_node.child_type = TPrimitiveType::DATE;
    expr_node.type = gen_type_desc(TPrimitiveType::DATETIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_DATE> col1(expr_node, 10, DateValue::create(2010, 10, 20));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_timestamp());

        // right cast
        auto v = std::static_pointer_cast<TimestampColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(TimestampValue::create(2010, 10, 20, 0, 0, 0), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, decimalCastString) {
    expr_node.child_type = TPrimitiveType::DECIMALV2;
    expr_node.type = gen_type_desc(TPrimitiveType::VARCHAR);
    expr_node.type.types[0].scalar_type.__set_len(10);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_DECIMALV2> col1(expr_node, 10, DecimalV2Value(123, 0));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_binary());

        // right cast
        auto v = std::static_pointer_cast<BinaryColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(std::string("123"), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, intCastString) {
    expr_node.child_type = TPrimitiveType::INT;
    expr_node.type = gen_type_desc(TPrimitiveType::VARCHAR);
    expr_node.type.types[0].scalar_type.__set_len(10);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_INT> col1(expr_node, 10, 12345);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_binary());

        // right cast
        auto v = std::static_pointer_cast<BinaryColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(std::string("12345"), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, booleanCastString) {
    expr_node.child_type = TPrimitiveType::BOOLEAN;
    expr_node.type = gen_type_desc(TPrimitiveType::VARCHAR);
    expr_node.type.types[0].scalar_type.__set_len(10);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_BOOLEAN> col1(expr_node, 10, true);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_binary());

        // right cast
        auto v = std::static_pointer_cast<BinaryColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(std::string("1"), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, timestmapCastString) {
    expr_node.child_type = TPrimitiveType::DATETIME;
    expr_node.type = gen_type_desc(TPrimitiveType::VARCHAR);
    expr_node.type.types[0].scalar_type.__set_len(10);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_DATETIME> col1(expr_node, 10, TimestampValue::create(2020, 02, 03, 1, 23, 45));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_binary());

        // right cast
        auto v = std::static_pointer_cast<BinaryColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(std::string("2020-02-03 01:23:45"), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastInt) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::INT);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("1234");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = ColumnHelper::cast_to_raw<TYPE_INT>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(1234, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastIntError) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::INT);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("123ad4");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_nullable());

        // right cast
        auto v = ColumnHelper::cast_to_raw<TYPE_INT>(ColumnHelper::as_raw_column<NullableColumn>(ptr)->data_column());
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_TRUE(ptr->is_null(j));
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastDouble) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DOUBLE);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("1234.1234");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = ColumnHelper::cast_to_raw<TYPE_DOUBLE>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(1234.1234, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastDoubleError) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DOUBLE);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("123ad4.123123");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_nullable());

        // right cast
        auto v =
                ColumnHelper::cast_to_raw<TYPE_DOUBLE>(ColumnHelper::as_raw_column<NullableColumn>(ptr)->data_column());
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_TRUE(ptr->is_null(j));
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastDecimal) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DECIMALV2);
    expr_node.type.types[0].scalar_type.__set_precision(10);
    expr_node.type.types[0].scalar_type.__set_scale(2);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    DecimalV2Value d(1794546454654654);
    std::string p = d.to_string();

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_decimal());

        // right cast
        auto v = ColumnHelper::cast_to_raw<TYPE_DECIMALV2>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(d, v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastDecimalError) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DECIMALV2);
    expr_node.type.types[0].scalar_type.__set_precision(10);
    expr_node.type.types[0].scalar_type.__set_scale(2);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("asdfadsf");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_nullable());

        // right cast
        auto v = ColumnHelper::cast_to_raw<TYPE_DECIMALV2>(
                ColumnHelper::as_raw_column<NullableColumn>(ptr)->data_column());
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_TRUE(ptr->is_null(j));
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastDate) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DATE);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("2023-12-02");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_date());

        // right cast
        auto v = ColumnHelper::cast_to_raw<TYPE_DATE>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(DateValue::create(2023, 12, 02), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastDate2) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DATE);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("   2023-12-02    ");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_date());

        // right cast
        auto v = ColumnHelper::cast_to_raw<TYPE_DATE>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(DateValue::create(2023, 12, 02), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastDateError) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DATE);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("2023-12-asdf");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_nullable());

        // right cast
        auto v = ColumnHelper::cast_to_raw<TYPE_DATE>(ColumnHelper::as_raw_column<NullableColumn>(ptr)->data_column());
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_TRUE(ptr->is_null(j));
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastTimestmap) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DATETIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("2022-02-03 11:23:45");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_timestamp());

        // right cast
        auto v = std::static_pointer_cast<TimestampColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(TimestampValue::create(2022, 02, 03, 11, 23, 45), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastTimestmap2) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DATETIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("    2022-02-03 11:23:45 ");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_timestamp());

        // right cast
        auto v = std::static_pointer_cast<TimestampColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(TimestampValue::create(2022, 02, 03, 11, 23, 45), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastTimestmap3) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DATETIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("2022-02-03     11:23:45");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_timestamp());

        // right cast
        auto v = std::static_pointer_cast<TimestampColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(TimestampValue::create(2022, 02, 03, 11, 23, 45), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastTimestmap4) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DATETIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("2022-02-03T11:23:45");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_timestamp());

        // right cast
        auto v = std::static_pointer_cast<TimestampColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(TimestampValue::create(2022, 02, 03, 11, 23, 45), v->get_data()[j]);
        }

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, stringCastTimestmapError) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::DATETIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("2022-02-03 asdfa");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        ASSERT_TRUE(ptr->is_timestamp());

        // right cast
        auto v = ColumnHelper::cast_to_raw<TYPE_DATETIME>(ptr);
        ASSERT_EQ(10, v->size());

        // error cast
        ASSERT_EQ(nullptr, std::dynamic_pointer_cast<Int64Column>(ptr));
    }
}

TEST_F(VectorizedCastExprTest, BigIntCastToInt) {
    expr_node.child_type = TPrimitiveType::BIGINT;
    expr_node.type = gen_type_desc(TPrimitiveType::INT);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_BIGINT> col1(expr_node, 10, INT64_MAX);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);
        ASSERT_TRUE(ptr->is_nullable());

        for (int j = 0; j < ptr->size(); ++j) {
            ASSERT_TRUE(ptr->is_null(j));
        }
    }
}

TEST_F(VectorizedCastExprTest, BigIntCastToInt2) {
    expr_node.child_type = TPrimitiveType::BIGINT;
    expr_node.type = gen_type_desc(TPrimitiveType::INT);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_BIGINT> col1(expr_node, 10, 10);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);
        ASSERT_TRUE(ptr->is_numeric());

        // right cast
        auto v = std::static_pointer_cast<Int32Column>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(10, v->get_data()[j]);
        }
    }
}

TEST_F(VectorizedCastExprTest, IntCastToBigInt3) {
    expr_node.child_type = TPrimitiveType::INT;
    expr_node.type = gen_type_desc(TPrimitiveType::BIGINT);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_INT> col1(expr_node, 10, INT_MAX);

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);
        ASSERT_FALSE(ptr->is_nullable());

        auto p = ColumnHelper::cast_to<TYPE_BIGINT>(ptr);
        for (int j = 0; j < p->size(); ++j) {
            ASSERT_EQ(INT_MAX, p->get_data()[j]);
        }
    }
}

TEST_F(VectorizedCastExprTest, stringCastToTime) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::TIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("15:15:15");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto v = std::static_pointer_cast<DoubleColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_EQ(54915, v->get_data()[j]);
        }
    }
}

TEST_F(VectorizedCastExprTest, stringCastToTimeNull1) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::TIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("15:15:15:");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto v = ColumnHelper::as_column<NullableColumn>(ptr);

        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_TRUE(v->is_null(j));
        }
    }
}

TEST_F(VectorizedCastExprTest, stringCastToTimeNull2) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::TIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("15:60:15");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto v = ColumnHelper::as_column<NullableColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_TRUE(v->is_null(j));
        }
    }
}

TEST_F(VectorizedCastExprTest, stringCastToTimeNull3) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::TIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("15:15");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto v = ColumnHelper::as_column<NullableColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_TRUE(v->is_null(j));
        }
    }
}

TEST_F(VectorizedCastExprTest, stringCastToTimeNull4) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::TIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("      :60:16");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto v = ColumnHelper::as_column<NullableColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_TRUE(v->is_null(j));
        }
    }
}

TEST_F(VectorizedCastExprTest, stringCastToTimeNull5) {
    expr_node.child_type = TPrimitiveType::VARCHAR;
    expr_node.type = gen_type_desc(TPrimitiveType::TIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    std::string p("15::15:15");

    MockVectorizedExpr<TYPE_VARCHAR> col1(expr_node, 10, Slice(p));

    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto v = ColumnHelper::as_column<NullableColumn>(ptr);
        ASSERT_EQ(10, v->size());

        for (int j = 0; j < v->size(); ++j) {
            ASSERT_TRUE(v->is_null(j));
        }
    }
}

TEST_F(VectorizedCastExprTest, bigintToTime) {
    expr_node.child_type = TPrimitiveType::BIGINT;
    expr_node.type = gen_type_desc(TPrimitiveType::TIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockMultiVectorizedExpr<TYPE_BIGINT> col1(expr_node, 2, 32020, 346050);
    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto v = ColumnHelper::as_column<NullableColumn>(ptr);
        ASSERT_EQ(2, v->size());

        auto d = ColumnHelper::cast_to<TYPE_TIME>(v->data_column());

        ASSERT_FALSE(v->is_null(0));
        ASSERT_EQ(12020, d->get_data()[0]);
        ASSERT_TRUE(v->is_null(1));
    }
}

TEST_F(VectorizedCastExprTest, dateToTime) {
    expr_node.child_type = TPrimitiveType::DATE;
    expr_node.type = gen_type_desc(TPrimitiveType::TIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));

    MockVectorizedExpr<TYPE_DATE> col1(expr_node, 2, DateValue::create(2000, 12, 01));
    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto d = ColumnHelper::cast_to<TYPE_TIME>(ptr);
        ASSERT_EQ(2, d->size());

        ASSERT_EQ(0, d->get_data()[0]);
        ASSERT_EQ(0, d->get_data()[1]);
    }
}

TEST_F(VectorizedCastExprTest, datetimeToTime) {
    expr_node.child_type = TPrimitiveType::DATETIME;
    expr_node.type = gen_type_desc(TPrimitiveType::TIME);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));
    MockVectorizedExpr<TYPE_DATETIME> col1(expr_node, 2, TimestampValue::create(2000, 12, 1, 12, 30, 00));
    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto d = ColumnHelper::cast_to<TYPE_TIME>(ptr);
        ASSERT_EQ(2, d->size());

        ASSERT_EQ(45000, d->get_data()[0]);
        ASSERT_EQ(45000, d->get_data()[1]);
    }
}

TEST_F(VectorizedCastExprTest, timeToInt) {
    expr_node.child_type = TPrimitiveType::TIME;
    expr_node.type = gen_type_desc(TPrimitiveType::INT);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));
    MockVectorizedExpr<TYPE_TIME> col1(expr_node, 2, 76862);
    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto d = ColumnHelper::cast_to<TYPE_INT>(ptr);
        ASSERT_EQ(2, d->size());

        ASSERT_EQ(212102, d->get_data()[0]);
        ASSERT_EQ(212102, d->get_data()[1]);
    }
}

TEST_F(VectorizedCastExprTest, timeToVarchar) {
    expr_node.child_type = TPrimitiveType::TIME;
    expr_node.type = gen_type_desc(TPrimitiveType::VARCHAR);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(expr_node));
    MockVectorizedExpr<TYPE_TIME> col1(expr_node, 2, 8521);
    expr->_children.push_back(&col1);

    {
        ColumnPtr ptr = expr->evaluate(nullptr, nullptr);

        // right cast
        auto d = ColumnHelper::cast_to<TYPE_VARCHAR>(ptr);
        ASSERT_EQ(2, d->size());

        ASSERT_EQ("02:22:01", d->get_data()[0]);
        ASSERT_EQ("02:22:01", d->get_data()[1]);
    }
}

template <PrimitiveType toType>
static typename RunTimeColumnType<toType>::Ptr evaluateCastFromJson(TExprNode& cast_expr, std::string json_str) {
    TPrimitiveType::type t_type = to_thrift(toType);
    cast_expr.type = gen_type_desc(t_type);

    std::cerr << "evaluate cast from json: " << json_str << std::endl;

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(cast_expr));
    auto json = JsonValue::parse(json_str);
    if (!json.ok()) {
        return nullptr;
    }
    MockVectorizedExpr<TYPE_JSON> col1(cast_expr, 2, &json.value());
    expr->_children.push_back(&col1);

    ColumnPtr ptr = expr->evaluate(nullptr, nullptr);
    if (!ptr) {
        return nullptr;
    }
    return ColumnHelper::cast_to<toType>(ptr);
}

template <PrimitiveType toType>
static ColumnPtr evaluateCastJsonNullable(TExprNode& cast_expr, std::string json_str) {
    std::cerr << "evaluate castCast: " << json_str << std::endl;
    TPrimitiveType::type t_type = to_thrift(toType);
    cast_expr.type = gen_type_desc(t_type);

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(cast_expr));
    if (!expr) {
        return nullptr;
    }
    auto json = JsonValue::parse(json_str);
    if (!json.ok()) {
        return nullptr;
    }
    MockVectorizedExpr<TYPE_JSON> col1(cast_expr, 2, &json.value());
    expr->_children.push_back(&col1);

    ColumnPtr ptr = expr->evaluate(nullptr, nullptr);
    return ptr;
}

// Test cast json value to SQL type
TEST_F(VectorizedCastExprTest, jsonToValue) {
    TExprNode cast_expr;
    cast_expr.opcode = TExprOpcode::CAST;
    cast_expr.child_type = TPrimitiveType::JSON;
    cast_expr.node_type = TExprNodeType::CAST_EXPR;
    cast_expr.num_children = 2;
    cast_expr.__isset.opcode = true;
    cast_expr.__isset.child_type = true;

    // cast self
    auto jsonCol = evaluateCastFromJson<TYPE_JSON>(cast_expr, "{\"a\": 1}");
    ASSERT_EQ("{\"a\": 1}", jsonCol->get_data()[0]->to_string().value());

    // cast success
    ASSERT_EQ(1, evaluateCastFromJson<TYPE_INT>(cast_expr, "1")->get_data()[0]);
    ASSERT_EQ(1.1, evaluateCastFromJson<TYPE_DOUBLE>(cast_expr, "1.1")->get_data()[0]);
    ASSERT_EQ(true, evaluateCastFromJson<TYPE_BOOLEAN>(cast_expr, "true")->get_data()[0]);
    ASSERT_EQ(false, evaluateCastFromJson<TYPE_BOOLEAN>(cast_expr, "false")->get_data()[0]);
    ASSERT_EQ("a", evaluateCastFromJson<TYPE_VARCHAR>(cast_expr, "\"a\"")->get_data()[0]);
    ASSERT_EQ("1", evaluateCastFromJson<TYPE_VARCHAR>(cast_expr, "\"1\"")->get_data()[0]);

    // implicit json type case
    ASSERT_EQ(1.0, evaluateCastFromJson<TYPE_DOUBLE>(cast_expr, "1")->get_data()[0]);
    ASSERT_EQ(1, evaluateCastFromJson<TYPE_INT>(cast_expr, "1.1")->get_data()[0]);

    // cast failed
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_INT>(cast_expr, "\"a\"")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_INT>(cast_expr, "false")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_INT>(cast_expr, "null")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_INT>(cast_expr, "[1,2]")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_BOOLEAN>(cast_expr, "1")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_BOOLEAN>(cast_expr, "\"a\"")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_BOOLEAN>(cast_expr, "1.0")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_BOOLEAN>(cast_expr, "null")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_BOOLEAN>(cast_expr, "[]")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_BOOLEAN>(cast_expr, "{}")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_VARCHAR>(cast_expr, "1.0")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_VARCHAR>(cast_expr, "null")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_VARCHAR>(cast_expr, "true")));
    ASSERT_EQ(2, ColumnHelper::count_nulls(evaluateCastJsonNullable<TYPE_VARCHAR>(cast_expr, "[1,2]")));

    // Not supported
    ASSERT_EQ(nullptr, evaluateCastJsonNullable<TYPE_DECIMALV2>(cast_expr, "1"));
    ASSERT_EQ(nullptr, evaluateCastJsonNullable<TYPE_DECIMAL32>(cast_expr, "1"));
    ASSERT_EQ(nullptr, evaluateCastJsonNullable<TYPE_DECIMAL64>(cast_expr, "1"));
    ASSERT_EQ(nullptr, evaluateCastJsonNullable<TYPE_DECIMAL128>(cast_expr, "1"));
    ASSERT_EQ(nullptr, evaluateCastJsonNullable<TYPE_TIME>(cast_expr, "1"));
    ASSERT_EQ(nullptr, evaluateCastJsonNullable<TYPE_DATE>(cast_expr, "1"));
    ASSERT_EQ(nullptr, evaluateCastJsonNullable<TYPE_DATETIME>(cast_expr, "1"));
    ASSERT_EQ(nullptr, evaluateCastJsonNullable<TYPE_HLL>(cast_expr, "1"));
}

template <PrimitiveType fromType>
static std::string evaluateCastToJson(TExprNode& cast_expr, RunTimeCppType<fromType> value) {
    cast_expr.child_type = to_thrift(fromType);
    cast_expr.type = gen_type_desc(to_thrift(TYPE_JSON));

    std::unique_ptr<Expr> expr(VectorizedCastExprFactory::from_thrift(cast_expr));
    if (!expr.get()) {
        return "";
    }
    MockVectorizedExpr<fromType> col1(cast_expr, 2, value);
    expr->_children.push_back(&col1);

    ColumnPtr ptr = expr->evaluate(nullptr, nullptr);
    if (!ptr) {
        return nullptr;
    }
    ColumnPtr result_column = ColumnHelper::cast_to<TYPE_JSON>(ptr);
    const JsonValue* json = result_column->get(0).get_json();
    auto json_str = json->to_string();
    if (!json_str.ok()) {
        return "";
    }
    return json_str.value();
}

// Test cast json value to SQL type
TEST_F(VectorizedCastExprTest, sqlToJson) {
    TExprNode cast_expr;
    cast_expr.opcode = TExprOpcode::CAST;
    cast_expr.node_type = TExprNodeType::CAST_EXPR;
    cast_expr.num_children = 2;
    cast_expr.__isset.opcode = true;
    cast_expr.__isset.child_type = true;

    // boolean
    {
        ASSERT_EQ("true", evaluateCastToJson<TYPE_BOOLEAN>(cast_expr, true));
        ASSERT_EQ("false", evaluateCastToJson<TYPE_BOOLEAN>(cast_expr, false));
    }
    // int
    {
        ASSERT_EQ("123", evaluateCastToJson<TYPE_INT>(cast_expr, 123));
        ASSERT_EQ("-123", evaluateCastToJson<TYPE_INT>(cast_expr, -123));
        ASSERT_EQ("-1", evaluateCastToJson<TYPE_TINYINT>(cast_expr, -1));
        ASSERT_EQ("-1", evaluateCastToJson<TYPE_SMALLINT>(cast_expr, -1));
        ASSERT_EQ("10000000000", evaluateCastToJson<TYPE_BIGINT>(cast_expr, 1E10));
        ASSERT_EQ("10000000000", evaluateCastToJson<TYPE_LARGEINT>(cast_expr, 1E10));
    }

    // double/float
    {
        ASSERT_EQ("1.23", evaluateCastToJson<TYPE_DOUBLE>(cast_expr, 1.23));
        ASSERT_EQ("-1.23", evaluateCastToJson<TYPE_DOUBLE>(cast_expr, -1.23));

        ASSERT_EQ("1.23", evaluateCastToJson<TYPE_FLOAT>(cast_expr, 1.23).substr(0, 4));
        ASSERT_EQ("-1.23", evaluateCastToJson<TYPE_FLOAT>(cast_expr, -1.23).substr(0, 5));
    }

    // string
    {
        std::string str = "star";
        ASSERT_EQ(R"("star")", evaluateCastToJson<TYPE_CHAR>(cast_expr, str));
        ASSERT_EQ(R"("star")", evaluateCastToJson<TYPE_VARCHAR>(cast_expr, str));

        str = "上海";
        ASSERT_EQ(R"("上海")", evaluateCastToJson<TYPE_CHAR>(cast_expr, str));
        ASSERT_EQ(R"("上海")", evaluateCastToJson<TYPE_VARCHAR>(cast_expr, str));
    }
    // json
    {
        JsonValue json = JsonValue::from_int(1);
        ASSERT_EQ(R"(1)", evaluateCastToJson<TYPE_JSON>(cast_expr, &json));
    }
}

} // namespace vectorized
} // namespace starrocks
